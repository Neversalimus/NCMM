"""Restore deferred configuration after the canonical stale-option repair layer.

That later layer replaces all four typed registration bodies and currently drops
the earlier eight deferred-value applications. Keep the canonical payload intact;
repair the exact final experimental source before any files are written.
"""
import re

def prepare(source):
    marker='// NCMM v8.7.6.6 stale world-option repair:'
    if source.count(marker)!=1 or source.count('static void ncmm_apply_deferred_option_value(')!=1:
        raise ValueError('Deferred Native options source identity changed')
    starts=list(re.finditer(r'(?m)^bool options_manager::ncmm_register_world_(bool|int|float|enum)\(',source))
    if [m.group(1) for m in starts]!=['bool','int','float','enum']:
        raise ValueError('Expected four exact Native setting registration bodies')
    end=source.find('\nvoid options_manager::update_global_locale()',starts[-1].end())
    if end<0:raise ValueError('Native settings end anchor changed')
    ranges=[(m.start(),starts[i+1].start() if i+1<len(starts) else end) for i,m in enumerate(starts)]
    for start,finish in reversed(ranges):
        body=source[start:finish]
        creation='        return true;\n    }\n    cOpt &opt = it->second;'
        refresh='    if( world_options.has_value() ) {'
        if body.count(creation)!=1 or body.count(refresh)!=1 or 'ncmm_apply_deferred_option_value(' in body:
            raise ValueError('Final Native setting repair body changed')
        body=body.replace(creation,'        ncmm_apply_deferred_option_value( name, options[name] );\n'+creation)
        body=body.replace(refresh,'    ncmm_apply_deferred_option_value( name, opt );\n'+refresh)
        source=source[:start]+body+source[finish:]
    return source
