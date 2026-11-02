#pragma once
#include <array>
#include <cmath>
#include <optional>
#include <vector>

// Investigation only. No renderer, resources, shader or Native code is changed.
namespace vehicle_body_union_detail {
using point=first_person::vehicle::point;
using vector=std::array<double,3>;
inline vector relative(point a,point b){return {{double(a.x)-b.x,double(a.y)-b.y,double(a.z)-b.z}};}
inline vector cross(vector a,vector b){return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};}
inline double dot(vector a,vector b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline bool same(point a,point b){return std::memcmp(&a,&b,sizeof(a))==0;}
struct polygon {
    std::vector<point> world;
    std::vector<size_t> sources,owners;
    std::vector<vector> edges;
    point eye{},forward{};
    double maximum_depth=0;
    static std::optional<polygon> make(std::vector<point> world,point eye,point forward){
        if(world.size()<3||world.size()>128)return {};
        const auto bounded=[](point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
            std::abs(p.x)<=512&&std::abs(p.y)<=512&&std::abs(p.z)<=512;};
        if(!bounded(eye)||!bounded(forward))return {};
        const double forward_length=double(forward.x)*forward.x+double(forward.y)*forward.y+double(forward.z)*forward.z;
        if(std::abs(forward_length-1)>1e-5)return {};
        for(size_t i=0;i<world.size();++i){if(!bounded(world[i]))return {};
            for(size_t j=0;j<i;++j)if(same(world[i],world[j]))return {};}

        polygon p;p.world=std::move(world);p.eye=eye;p.forward=forward;
        const vector f{{forward.x,forward.y,forward.z}};std::vector<vector> rays;vector center{};
        for(auto at:p.world){const auto r=relative(at,eye);const double z=dot(r,f);
            if(!std::isfinite(z)||z<=.15)return {};
            p.maximum_depth=std::max(p.maximum_depth,z);rays.push_back(r);
            for(unsigned i=0;i<3;++i)center[i]+=r[i]/p.world.size();}
        double orientation=0;
        for(size_t i=0;i<rays.size();++i)orientation+=dot(cross(rays[i],rays[(i+1)%rays.size()]),center);
        if(std::abs(orientation)<1e-10)return {};
        if(orientation<0){std::reverse(p.world.begin(),p.world.end());std::reverse(rays.begin(),rays.end());}
        for(size_t i=0;i<rays.size();++i){auto normal=cross(rays[i],rays[(i+1)%rays.size()]);const double length=std::sqrt(dot(normal,normal));
            if(!std::isfinite(length)||length<1e-9)return {};
            for(auto &n:normal)n/=length;
            if(dot(normal,center)<=1e-9)return {};
            // Collinear boundary vertices are retained, never replaced with a
            // convex hull. The strict later2cm coverage margin dominates this
            // round-off allowance for an exact shared-edge union.
            for(const auto &ray:rays)if(dot(normal,ray)<-1e-9)return {};
            p.edges.push_back(normal);}
        return p;
    }
    bool hides(size_t owner,point low,point high)const{
        const auto bounded=[](point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
            std::abs(p.x)<=512&&std::abs(p.y)<=512&&std::abs(p.z)<=512;};
        if(!bounded(low)||!bounded(high)||low.x>high.x||low.y>high.y||low.z>high.z||
            std::find(owners.begin(),owners.end(),owner)!=owners.end())return false;
        const vector center{{(double(low.x)+high.x)/2-eye.x,(double(low.y)+high.y)/2-eye.y,(double(low.z)+high.z)/2-eye.z}},
            half{{(double(high.x)-low.x)/2,(double(high.y)-low.y)/2,(double(high.z)-low.z)/2}},f{{forward.x,forward.y,forward.z}};
        const auto radius=[&](vector n){return std::abs(n[0])*half[0]+std::abs(n[1])*half[1]+std::abs(n[2])*half[2];};
        if(dot(f,center)-radius(f)<=maximum_depth+.02)return false;
        for(const auto &n:edges)if(dot(n,center)-radius(n)<=.02)return false;
        return true;
    }
};
template<class Quad>std::vector<polygon> collect(const std::vector<Quad> &quads,point eye,point forward){
    std::vector<polygon> out;
    for(size_t i=0;i<quads.size();++i){const auto &q=quads[i];auto p=polygon::make(std::vector<point>(q.world.begin(),q.world.end()),eye,forward);
        if(!p)continue;p->sources.push_back(i);p->owners.push_back(q.owner);out.push_back(std::move(*p));}
    bool changed=true;
    while(changed){changed=false;
        for(size_t i=0;i<out.size()&&!changed;++i)for(size_t j=i+1;j<out.size()&&!changed;++j){
            const auto &a=out[i].world,&b=out[j].world;
            for(size_t ai=0;ai<a.size()&&!changed;++ai)for(size_t bi=0;bi<b.size()&&!changed;++bi){
                if(!same(a[ai],b[(bi+1)%b.size()])||!same(a[(ai+1)%a.size()],b[bi]))continue;
                std::vector<point> boundary;
                for(size_t k=0;k<a.size();++k)boundary.push_back(a[(ai+1+k)%a.size()]);
                for(size_t k=1;k+1<b.size();++k)boundary.push_back(b[(bi+1+k)%b.size()]);
                auto united=polygon::make(std::move(boundary),eye,forward);if(!united)continue;
                united->sources=out[i].sources;united->sources.insert(united->sources.end(),out[j].sources.begin(),out[j].sources.end());
                united->owners=out[i].owners;united->owners.insert(united->owners.end(),out[j].owners.begin(),out[j].owners.end());
                std::sort(united->owners.begin(),united->owners.end());united->owners.erase(std::unique(united->owners.begin(),united->owners.end()),united->owners.end());
                out[i]=std::move(*united);out.erase(out.begin()+j);changed=true;
            }
        }
    }
    return out;
}
}
