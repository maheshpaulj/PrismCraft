import os
import json
import soundfile as sf
import numpy as np

def main():
    appdata = os.environ.get('APPDATA')
    if not appdata:
        print("APPDATA environment variable not found")
        return

    index_path = os.path.join(appdata, '.minecraft', 'assets', 'indexes', '33.json')
    if not os.path.exists(index_path):
        print(f"Index file not found: {index_path}")
        return

    with open(index_path, 'r') as f:
        data = json.load(f)

    objs = data.get('objects', {})
    objects_dir = os.path.join(appdata, '.minecraft', 'assets', 'objects')

    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sounds_dir = os.path.join(base_dir, 'assets', 'sounds')
    music_dir = os.path.join(base_dir, 'assets', 'music')
    os.makedirs(sounds_dir, exist_ok=True)
    os.makedirs(music_dir, exist_ok=True)

    # Release target directories as well
    rel_sounds_dir = os.path.join(base_dir, 'x64', 'Release', 'assets', 'sounds')
    rel_music_dir = os.path.join(base_dir, 'x64', 'Release', 'assets', 'music')
    os.makedirs(rel_sounds_dir, exist_ok=True)
    os.makedirs(rel_music_dir, exist_ok=True)

    # Sound map: (target_filename, list_of_possible_keys)
    sound_mappings = {
        # UI
        'click.wav': ['minecraft/sounds/random/click.ogg'],
        'pop.wav': ['minecraft/sounds/random/pop.ogg'],
        
        # Combat & Actions
        'bow_shoot.wav': ['minecraft/sounds/random/bow.ogg'],
        'bow_hit1.wav': ['minecraft/sounds/random/bowhit1.ogg'],
        'bow_hit2.wav': ['minecraft/sounds/random/bowhit2.ogg'],
        'bow_hit3.wav': ['minecraft/sounds/random/bowhit3.ogg'],
        'bow_hit4.wav': ['minecraft/sounds/random/bowhit4.ogg'],
        'hurt1.wav': ['minecraft/sounds/damage/hit1.ogg'],
        'hurt2.wav': ['minecraft/sounds/damage/hit2.ogg'],
        'hurt3.wav': ['minecraft/sounds/damage/hit3.ogg'],
        'fall.wav': ['minecraft/sounds/damage/fallsmall.ogg'],
        'splash.wav': ['minecraft/sounds/liquid/splash.ogg'],
        'swim1.wav': ['minecraft/sounds/liquid/swim1.ogg'],
        'swim2.wav': ['minecraft/sounds/liquid/swim2.ogg'],
        'swim3.wav': ['minecraft/sounds/liquid/swim3.ogg'],
        'swim4.wav': ['minecraft/sounds/liquid/swim4.ogg'],

        # Footsteps
        'grass_step1.wav': ['minecraft/sounds/step/grass1.ogg'],
        'grass_step2.wav': ['minecraft/sounds/step/grass2.ogg'],
        'grass_step3.wav': ['minecraft/sounds/step/grass3.ogg'],
        'grass_step4.wav': ['minecraft/sounds/step/grass4.ogg'],
        'stone_step1.wav': ['minecraft/sounds/step/stone1.ogg'],
        'stone_step2.wav': ['minecraft/sounds/step/stone2.ogg'],
        'stone_step3.wav': ['minecraft/sounds/step/stone3.ogg'],
        'stone_step4.wav': ['minecraft/sounds/step/stone4.ogg'],
        'wood_step1.wav': ['minecraft/sounds/step/wood1.ogg'],
        'wood_step2.wav': ['minecraft/sounds/step/wood2.ogg'],
        'wood_step3.wav': ['minecraft/sounds/step/wood3.ogg'],
        'wood_step4.wav': ['minecraft/sounds/step/wood4.ogg'],
        'sand_step1.wav': ['minecraft/sounds/step/sand1.ogg'],
        'sand_step2.wav': ['minecraft/sounds/step/sand2.ogg'],
        'sand_step3.wav': ['minecraft/sounds/step/sand3.ogg'],
        'sand_step4.wav': ['minecraft/sounds/step/sand4.ogg'],

        # Digging / Breaking
        'grass_dig1.wav': ['minecraft/sounds/dig/grass1.ogg'],
        'grass_dig2.wav': ['minecraft/sounds/dig/grass2.ogg'],
        'grass_dig3.wav': ['minecraft/sounds/dig/grass3.ogg'],
        'grass_dig4.wav': ['minecraft/sounds/dig/grass4.ogg'],
        'stone_dig1.wav': ['minecraft/sounds/dig/stone1.ogg'],
        'stone_dig2.wav': ['minecraft/sounds/dig/stone2.ogg'],
        'stone_dig3.wav': ['minecraft/sounds/dig/stone3.ogg'],
        'stone_dig4.wav': ['minecraft/sounds/dig/stone4.ogg'],
        'wood_dig1.wav': ['minecraft/sounds/dig/wood1.ogg'],
        'wood_dig2.wav': ['minecraft/sounds/dig/wood2.ogg'],
        'wood_dig3.wav': ['minecraft/sounds/dig/wood3.ogg'],
        'wood_dig4.wav': ['minecraft/sounds/dig/wood4.ogg'],
        'sand_dig1.wav': ['minecraft/sounds/dig/sand1.ogg'],
        'sand_dig2.wav': ['minecraft/sounds/dig/sand2.ogg'],
        'sand_dig3.wav': ['minecraft/sounds/dig/sand3.ogg'],
        'sand_dig4.wav': ['minecraft/sounds/dig/sand4.ogg'],
    }

    music_mappings = {
        'sweden.wav': ['minecraft/sounds/music/game/sweden.ogg'],
        'minecraft.wav': ['minecraft/sounds/music/game/minecraft.ogg'],
        'subwoofer_lullaby.wav': ['minecraft/sounds/music/game/subwoofer_lullaby.ogg']
    }

    target_sr = 44100

    def extract_and_convert(mappings, dest_dirs):
        for out_name, keys in mappings.items():
            src_hash = None
            for k in keys:
                if k in objs:
                    src_hash = objs[k]['hash']
                    break
            if not src_hash:
                print(f"Warning: could not find key for {out_name}")
                continue

            src_file = os.path.join(objects_dir, src_hash[:2], src_hash)
            if not os.path.exists(src_file):
                print(f"Warning: file {src_file} missing for {out_name}")
                continue

            try:
                data_in, in_sr = sf.read(src_file)
                # Convert to mono if multi-channel
                if len(data_in.shape) > 1 and data_in.shape[1] > 1:
                    data_in = np.mean(data_in, axis=1)

                # Resample to 44100 if needed
                if in_sr != target_sr:
                    from scipy import signal
                    num_samples = int(len(data_in) * target_sr / in_sr)
                    data_in = signal.resample(data_in, num_samples)

                # Convert to int16 range
                data_in = np.clip(data_in, -1.0, 1.0)
                int_data = (data_in * 32767.0).astype(np.int16)

                for d in dest_dirs:
                    out_path = os.path.join(d, out_name)
                    sf.write(out_path, int_data, target_sr, subtype='PCM_16')
                print(f"Extracted: {out_name} ({len(int_data)} samples)")
            except Exception as e:
                print(f"Error converting {out_name}: {e}")

    print("Extracting sound effects...")
    extract_and_convert(sound_mappings, [sounds_dir, rel_sounds_dir])

    print("\nExtracting music...")
    extract_and_convert(music_mappings, [music_dir, rel_music_dir])

    print("\nAudio extraction complete!")

if __name__ == '__main__':
    main()
