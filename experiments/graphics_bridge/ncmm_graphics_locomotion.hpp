#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ncmm_graphics_camera {
// Fractional presentation position relative to the authoritative Native cell.
// Crossing requests one action; no map, clock, actor or action queue lives here.
class locomotion {
public:
    struct point {float x=0,y=0;};
    struct crossing {int x=0,y=0;explicit operator bool()const{return x||y;}};
    static constexpr float radius=.18f;
private:
    point position_{},velocity_{},remainder_{};crossing boundary_{};
    uint64_t clock_=0;bool clock_ready_=false;
    float distance_=0;
    template<class Blocked> bool fits(point p,Blocked blocked)const{
        const float x=p.x+.5f,y=p.y+.5f;
        for(int cy=int(std::floor(y-radius));cy<=int(std::floor(y+radius));++cy)
            for(int cx=int(std::floor(x-radius));cx<=int(std::floor(x+radius));++cx){
                if((!cx&&!cy)||!blocked(cx,cy))continue;
                const float dx=x-std::clamp(x,float(cx),float(cx+1)),dy=y-std::clamp(y,float(cy),float(cy+1));
                if(dx*dx+dy*dy<radius*radius-1e-7f)return false;
            }
        return true;
    }
    bool clip_boundary(point from,point &to){
        float amount=1;crossing step{};
        auto side=[&](float a,float b,int axis){
            if(b>.5f||(b>=.5f&&b>a)){
                const float t=(.5f-a)/(b-a);
                if(t<amount-1e-5f){amount=t;step={};}
                if(t<=amount+1e-5f)(axis?step.y:step.x)=1;
            }else if(b<-.5f||(b<=-.5f&&b<a)){
                const float t=(-.5f-a)/(b-a);
                if(t<amount-1e-5f){amount=t;step={};}
                if(t<=amount+1e-5f)(axis?step.y:step.x)=-1;
            }
        };
        side(from.x,to.x,0);side(from.y,to.y,1);
        if(!step)return false;
        amount=std::clamp(amount,0.f,1.f);
        to={from.x+(to.x-from.x)*amount,from.y+(to.y-from.y)*amount};
        // sin/cos at exact diagonal headings differ by a few float ULPs.
        // Merge boundaries within 0.00001 world units, rather than making
        // a nominal diagonal spend two Native actions at a frame-dependent gap.
        constexpr float corner_epsilon=.00001f;
        if(to.x>=.5f-corner_epsilon&&to.x>from.x)step.x=1;
        else if(to.x<=-.5f+corner_epsilon&&to.x<from.x)step.x=-1;
        if(to.y>=.5f-corner_epsilon&&to.y>from.y)step.y=1;
        else if(to.y<=-.5f+corner_epsilon&&to.y<from.y)step.y=-1;
        if(step.x)to.x=.5f*step.x;
        if(step.y)to.y=.5f*step.y;
        boundary_=step;return true;
    }
public:
    point position()const{return position_;}
    point velocity()const{return velocity_;}
    crossing pending()const{return boundary_;}
    float distance()const{return distance_;}
    float speed()const{return std::hypot(velocity_.x,velocity_.y);}
    bool moving()const{return speed()>.005f||bool(boundary_);}
    void clock(uint64_t now){clock_=now;clock_ready_=true;}
    void stop(uint64_t now){velocity_={};remainder_={};clock(now);}
    void pause(uint64_t now){boundary_={};position_.x=std::clamp(position_.x,-.49999f,.49999f);position_.y=std::clamp(position_.y,-.49999f,.49999f);stop(now);}
    void reset(uint64_t now=0){position_={};velocity_={};remainder_={};boundary_={};distance_=0;clock_ready_=false;clock_=now;}
    bool acknowledge(int x,int y){
        if(!boundary_||boundary_.x!=x||boundary_.y!=y)return false;
        position_.x-=float(x);position_.y-=float(y);boundary_={};return true;
    }
    void reject(uint64_t now){
        boundary_={};position_.x=std::clamp(position_.x,-.5f+radius,.5f-radius);
        position_.y=std::clamp(position_.y,-.5f+radius,.5f-radius);stop(now);
    }
    // A newly closed door or moved obstacle may invalidate an old fractional
    // position. Move only as far toward the cell centre as collision requires.
    template<class Blocked> void constrain(Blocked blocked,uint64_t now){
        if(fits(position_,blocked))return;
        float lo=0,hi=1;
        for(int n=0;n<16;++n){const float t=(lo+hi)*.5f;
            if(fits({position_.x*t,position_.y*t},blocked))lo=t;else hi=t;}
        position_.x*=lo;position_.y*=lo;boundary_={};stop(now);
    }
    template<class Blocked> bool advance(uint64_t now,float yaw,int forward,int right,float speed,Blocked blocked){
        if(!clock_ready_){clock(now);return false;}
        const float dt=now<clock_?0.f:std::min(.05f,float(now-clock_)*.001f);clock(now);
        if(boundary_||dt<=0)return false;
        if(!std::isfinite(yaw)||!std::isfinite(speed)||speed<0){stop(now);return false;}
        speed=std::min(speed,8.f);
        const float f=float(std::clamp(forward,-1,1)),r=float(std::clamp(right,-1,1)),length=std::hypot(f,r);
        point target{};
        if(length>0){const float c=std::cos(yaw),s=std::sin(yaw);
            target={(c*f-s*r)*speed/length,(s*f+c*r)*speed/length};}
        // Exact integration of a first-order velocity response gives the same
        // acceleration/braking over ordinary 30/60/144 Hz frame sequences.
        const float rate=length>0?22.f:38.f,decay=std::exp(-rate*dt),integral=(1-decay)/rate;
        const point shift{target.x*dt+(velocity_.x-target.x)*integral+remainder_.x,target.y*dt+(velocity_.y-target.y)*integral+remainder_.y};
        remainder_={};
        velocity_={target.x+(velocity_.x-target.x)*decay,target.y+(velocity_.y-target.y)*decay};
        const int count=std::clamp(int(std::ceil(std::hypot(shift.x,shift.y)/.025f)),1,24);
        const point delta{shift.x/count,shift.y/count};bool changed=false;
        for(int n=0;n<count;++n){const point before=position_;point next{before.x+delta.x,before.y+delta.y};
            if(!fits(next,blocked)){
                next=before;
                const point horizontal{before.x+delta.x,before.y};
                if(fits(horizontal,blocked))next.x=horizontal.x;else velocity_.x=0;
                const point vertical{next.x,before.y+delta.y};
                if(fits(vertical,blocked))next.y=vertical.y;else velocity_.y=0;
            }
            const point unclipped=next;const bool crossing=clip_boundary(before,next);
            distance_+=std::hypot(next.x-before.x,next.y-before.y);
            changed=changed||std::abs(next.x-before.x)+std::abs(next.y-before.y)>1e-7f;
            position_=next;if(crossing){
                remainder_={velocity_.x==0?0:unclipped.x-next.x+float(count-n-1)*delta.x,
                            velocity_.y==0?0:unclipped.y-next.y+float(count-n-1)*delta.y};break;
            }
        }
        return changed;
    }
};
}
