"""Compare actual window pixels in the fixed-camera native firing fixture.

This is a paired fixture check, not a general image classifier or manual play.
The structural verifier still checks native commands, ammunition and all PNGs.
"""
import argparse
import json
from pathlib import Path
from PIL import Image
from verify_fire_audit import verify as verify_structure

def samples(path, coordinates):
    with Image.open(path) as image:
        assert image.format == 'PNG' and 0 < image.width <= 8192 and 0 < image.height <= 8192
        image = image.convert('RGB')
        return [image.getpixel((min(image.width-1, int(x*image.width)), min(image.height-1, int(y*image.height)))) for x, y in coordinates]

def distance(a, b):
    return max(abs(x-y) for x, y in zip(a, b))

def verify_spatial(root, trials, frames):
    root = Path(root)
    controls = [trial for trial in trials if trial['legacy_control']]
    assert len(controls) == 1
    control = controls[0]
    first, last = control['first_frame'], control['last_frame_exclusive']
    image = lambda index: root/f'first-person-fire-frame-{index}.png'
    # Prefer the left side of the world viewport, clear of native targeting UI.
    # Select only spatial points that actually distinguish this run's known
    # pre-input perspective from its observed legacy map flash.
    candidates = [(x/100, y/100) for x in range(5, 61, 5) for y in [8, 15, 22]]
    reference = samples(image(first), candidates)
    legacy = [index for index in range(first, last) if not frames[index]['perspective']]
    assert legacy
    bad_index = max(legacy, key=lambda index: sum(distance(a, b) > 64 for a, b in zip(reference, samples(image(index), candidates))))
    bad = samples(image(bad_index), candidates)
    selected = [point for point, a, b in zip(candidates, reference, bad) if distance(a, b) > 64][:18]
    assert len(selected) >= 12, 'Legacy flags do not prove an observable map flash in the actual pixels'
    records = []
    for trial in trials:
        if trial['legacy_control']:
            continue
        start, end = trial['first_frame'], trial['last_frame_exclusive']
        # A real foreground window/wall may occupy the control's open-sky
        # samples. Select equally strict discriminators from this trial's own
        # pre-input perspective, without discarding any presented frame.
        trial_reference = samples(image(start), candidates)
        selected_trial = [point for point, a, b in zip(candidates, trial_reference, bad) if distance(a, b) > 64][:18]
        assert len(selected_trial) >= 12, 'Trial reference does not distinguish the visible map pixels'
        expected = samples(image(start), selected_trial)
        minimum = 1.0
        for index in range(start, end):
            actual = samples(image(index), selected_trial)
            matched = sum(distance(a, b) <= 24 for a, b in zip(expected, actual))/len(selected_trial)
            minimum = min(minimum, matched)
            assert matched >= .75, f'World pixels diverged from the pre-input perspective: frame {index}, match {matched:.3f}'
        records.append(dict(scenario=trial['scenario'], renderer=trial['renderer'], first=start, last_exclusive=end,
                            minimum_reference_match=minimum,sample_coordinates=selected_trial))
    return dict(success=True, manual_playthrough=False, method='per-trial paired fixed-camera spatial world patches',
                legacy_visible_map_frame=bad_index, sample_coordinates=selected, trials_checked=len(records),
                frames_checked=sum(record['last_exclusive']-record['first'] for record in records), records=records,
                limitations=['Purpose-built native fixture; does not classify arbitrary gameplay or replace visual review'])

def verify(root):
    structural = verify_structure(root)
    report = json.loads((Path(root)/'first-person-fire-audit.json').read_text(encoding='utf-8-sig'))
    result = verify_spatial(root, report['trials'], report['frames'])
    result.update(schema=1, complete_audit=True, confirmed_shots=structural['confirmed_shots'])
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.root)
    args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({key: value for key, value in result.items() if key not in ['records', 'sample_coordinates']}))
