# Re-import the footstep WAVs over the existing assets.
#
# Why an import TASK rather than deleting and re-adding the assets: the sound
# sets reference these SoundWaves by object path. Importing with
# replace_existing keeps the same objects, so every reference survives and the
# new audio simply appears inside them. Deleting first would empty the sets.
import unreal

SRC = r"D:\UnrealProjects\Seawall\Content\Audio\Player\Footsteps\Concrete"
DEST = "/Game/Audio/Player/Footsteps/Concrete"

import os

wavs = sorted(os.path.join(SRC, f) for f in os.listdir(SRC) if f.lower().endswith(".wav"))
print("re-importing %d files" % len(wavs))

tasks = []
for path in wavs:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

imported = 0
for task in tasks:
    imported += len(task.get_editor_property("imported_object_paths") or [])
print("imported object paths: %d" % imported)

# The sets keep their references, so confirm one of them still has its takes
# and that the new audio really is in there (durations changed in the rewrite).
lib = unreal.EditorAssetLibrary
for name in ("SS_Step_Concrete_Walk", "SS_Step_Concrete_Sprint", "SS_Step_Concrete_LandHard"):
    ss = lib.load_asset("/Game/Audio/Data/%s" % name)
    sounds = ss.get_editor_property("sounds")
    durations = [round(s.get_editor_property("duration"), 3) for s in sounds if s]
    print("%-28s %2d takes, durations %s" % (name, len(sounds), durations[:4]))

# Tighter pitch spread now that there is no tone to expose.
for name, lo, hi in (("SS_Step_Concrete_Walk", 0.96, 1.04),
                     ("SS_Step_Concrete_Sprint", 0.95, 1.05),
                     ("SS_Step_Concrete_Crouch", 0.97, 1.03),
                     ("SS_Step_Concrete_Jump", 0.96, 1.04),
                     ("SS_Step_Concrete_LandSoft", 0.96, 1.04),
                     ("SS_Step_Concrete_LandHard", 0.95, 1.05),
                     ("SS_Step_Concrete_Scuff", 0.92, 1.08)):
    path = "/Game/Audio/Data/%s" % name
    ss = lib.load_asset(path)
    ss.set_editor_property("pitch_range", unreal.Vector2D(lo, hi))
    lib.save_asset(path)

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
print("done")
