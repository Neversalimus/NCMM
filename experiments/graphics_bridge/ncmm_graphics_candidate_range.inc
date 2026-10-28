// Included at the loaded candidate evaluation site. Keep Native query order
// and integer distance policy; these locals do not survive the candidate.
// Surface-light samples change illumination, not the target XYZ. Keep
// Native's integer result and distance policy for this candidate only.
const int wanted=rl_dist(you.pos_bub(),p);
const auto below=tripoint_bub_ms(p.xy(),z-1);
const bool current_in_range=graphics_world_in_visual_range(p,visual_maximum,visual_no_sight,&visual_ranges,nullptr,wanted);
// Architecture is continuous inside the physical sight radius, even
// when a dark room/tile centre has never passed the top-down FOV test.
// Its light and the raster depth still determine what reaches the eye.
const bool structure_range=wanted<=(visual_no_sight?1:visual_maximum);
const bool static_surface=structure_range&&(semantic&(graphics_world_architecture|NCMM_WORLD_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_DOWN|NCMM_WORLD_UP));
// Static surfaces also imply structure_range. Both later lower consumers
// short-circuit here; a nonstatic candidate still needs its exact lower query.
const bool lower_in_range=!static_surface&&in_visual_range(below);
