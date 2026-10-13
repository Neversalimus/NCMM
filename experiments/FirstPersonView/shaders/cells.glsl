// Included only after the callback-owned Cells storage declaration.
// Sparse cells are sorted by logical (z,y,x); positions use physical Z.
int world_cell_index(vec3 p){
    if(camera.grid.w<.5)return cell_index(ivec2(floor(p.xy)));
    ivec3 wanted=ivec3(floor(p.xy),floor(p.z/2.5));
    int lo=0,hi=int(camera.grid.z);
    while(lo<hi){int middle=(lo+hi)/2;ivec3 c=ivec3(cells[middle].origin.x,cells[middle].origin.y,cells[middle].origin.w);
        bool before=c.z<wanted.z||(c.z==wanted.z&&(c.y<wanted.y||(c.y==wanted.y&&c.x<wanted.x)));
        if(before)lo=middle+1;else hi=middle;}
    if(lo>=int(camera.grid.z))return -1;
    ivec3 found=ivec3(cells[lo].origin.x,cells[lo].origin.y,cells[lo].origin.w);
    return all(equal(found,wanted))?lo:-1;
}
