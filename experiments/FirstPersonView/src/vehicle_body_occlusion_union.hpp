#pragma once
#include "vehicle_body_occlusion_guard.hpp"
#include <map>
#include <set>
#include <tuple>

namespace first_person::vehicle::body_occlusion_detail {
struct edge_ref {size_t root=0,generation=0,edge=0;};
using vertex_bits=std::array<uint32_t,3>;
inline vertex_bits bits(point p){vertex_bits out{};std::memcpy(out.data(),&p,sizeof(p));return out;}
template<class Quad>std::vector<polygon> join(const std::vector<Quad> &quads,point eye,point forward){
    std::vector<polygon> roots;std::vector<size_t> generations;std::vector<bool> active;
    roots.reserve(quads.size());generations.reserve(quads.size());active.reserve(quads.size());
    for(size_t i=0;i<quads.size();++i){const auto &q=quads[i];auto p=polygon::make(std::vector<point>(q.world.begin(),q.world.end()),eye,forward);
        if(!p)continue;p->sources.push_back(i);p->owners.push_back(q.owner);roots.push_back(std::move(*p));generations.push_back(0);active.push_back(true);}
    using edge=std::pair<vertex_bits,vertex_bits>;
    std::map<edge,std::vector<edge_ref>> edges;
    // Stable root and edge order reproduces the old exhaustive greedy result.
    using candidate=std::tuple<size_t,size_t,size_t,size_t,size_t,size_t>;
    std::set<candidate> pending;
    const auto add=[&](size_t root){const auto &world=roots[root].world;
        for(size_t i=0;i<world.size();++i){const edge key{bits(world[i]),bits(world[(i+1)%world.size()])},reverse{key.second,key.first};
            const auto found=edges.find(reverse);
            if(found!=edges.end())for(const auto &ref:found->second){
                if(ref.root==root||!active[ref.root]||ref.generation!=generations[ref.root])continue;
                if(root<ref.root)pending.emplace(root,ref.root,i,ref.edge,generations[root],ref.generation);
                else pending.emplace(ref.root,root,ref.edge,i,ref.generation,generations[root]);}
            edges[key].push_back({root,generations[root],i});}
    };
    for(size_t i=0;i<roots.size();++i)add(i);
    while(!pending.empty()){
        const auto [i,j,ai,bi,gi,gj]=*pending.begin();pending.erase(pending.begin());
        if(!active[i]||!active[j]||gi!=generations[i]||gj!=generations[j])continue;
        const auto &a=roots[i].world,&b=roots[j].world;std::vector<point> boundary;boundary.reserve(a.size()+b.size()-2);
        for(size_t k=0;k<a.size();++k)boundary.push_back(a[(ai+1+k)%a.size()]);
        for(size_t k=1;k+1<b.size();++k)boundary.push_back(b[(bi+1+k)%b.size()]);
        auto united=polygon::make(std::move(boundary),eye,forward);if(!united)continue;
        united->sources=roots[i].sources;united->sources.insert(united->sources.end(),roots[j].sources.begin(),roots[j].sources.end());
        united->owners=roots[i].owners;united->owners.insert(united->owners.end(),roots[j].owners.begin(),roots[j].owners.end());
        std::sort(united->owners.begin(),united->owners.end());united->owners.erase(std::unique(united->owners.begin(),united->owners.end()),united->owners.end());
        roots[i]=std::move(*united);++generations[i];active[j]=false;++generations[j];add(i);
    }
    std::vector<polygon> result;result.reserve(roots.size());for(size_t i=0;i<roots.size();++i)if(active[i])result.push_back(std::move(roots[i]));
    return result;
}
}
