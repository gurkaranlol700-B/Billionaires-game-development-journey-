# SEAWALL -- Phase 0 spike: can editor Python create audio assets?
#
# Run inside the Unreal editor (Output Log command box, "Cmd" mode):
#     py "D:/UnrealProjects/Seawall/Tools/Audio/spike_editor_python.py"
#
# It tries the two routes the audio pass depends on and records what happened
# (including the Python signatures Unreal generated) in
# Saved/AudioSpike.json, so a failure still tells us how to fix the call.
# Everything it makes lives in /Game/Audio/_Spike and can be deleted afterwards.

import json
import traceback

import unreal

SPIKE_PATH = "/Game/Audio/_Spike"
REPORT = unreal.Paths.project_saved_dir() + "AudioSpike.json"
report = {"steps": [], "docs": {}}


def step(name, ok, detail=""):
    report["steps"].append({"step": name, "ok": ok, "detail": str(detail)})
    unreal.log(f"[SW-SPIKE] {name}: {'OK' if ok else 'FAILED'} {detail}")


def doc(obj, fn_name):
    fn = getattr(obj, fn_name, None)
    report["docs"][fn_name] = fn.__doc__ if fn is not None else "<missing>"


# --- 1. A plain SoundClass through its asset factory ------------------------
try:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    sc_path = f"{SPIKE_PATH}/SC_Spike"
    if unreal.EditorAssetLibrary.does_asset_exist(sc_path):
        step("SoundClass", True, "already exists")
    else:
        sc = tools.create_asset("SC_Spike", SPIKE_PATH, unreal.SoundClass, unreal.SoundClassFactory())
        step("SoundClass", sc is not None, sc)
except Exception:
    step("SoundClass", False, traceback.format_exc())

# --- 2. A MetaSound Source built node by node --------------------------------
try:
    builder_sub = unreal.get_engine_subsystem(unreal.MetaSoundBuilderSubsystem)
    editor_sub = unreal.get_editor_subsystem(unreal.MetaSoundEditorSubsystem)
    for fn in ("create_source_builder", "create_float_meta_sound_literal"):
        doc(builder_sub, fn)
    doc(editor_sub, "build_to_asset")

    created = builder_sub.create_source_builder("SW_SpikeBuilder", unreal.MetaSoundOutputAudioFormat.MONO, False)
    step("create_source_builder returned", True, repr(created))
    builder, on_play, on_finished, audio_outs = created[0], created[1], created[2], created[3]
    for fn in ("add_node_by_class_name", "find_node_input_by_name", "find_node_output_by_name",
               "connect_nodes", "set_node_input_default"):
        doc(builder, fn)

    class_name = unreal.MetasoundFrontendClassName()
    class_name.set_editor_property("namespace", "UE")
    class_name.set_editor_property("name", "Sine")
    class_name.set_editor_property("variant", "Audio")

    osc = builder.add_node_by_class_name(class_name, 1)
    step("add Sine node", True, repr(osc))
    osc = osc[0] if isinstance(osc, tuple) else osc

    freq_in = builder.find_node_input_by_name(osc, "Frequency")
    freq_in = freq_in[0] if isinstance(freq_in, tuple) else freq_in
    literal = builder_sub.create_float_meta_sound_literal(110.0)
    literal = literal[0] if isinstance(literal, tuple) else literal
    builder.set_node_input_default(freq_in, literal)
    step("set Frequency = 110", True)

    audio_out = builder.find_node_output_by_name(osc, "Audio")
    audio_out = audio_out[0] if isinstance(audio_out, tuple) else audio_out
    builder.connect_nodes(audio_out, audio_outs[0])
    step("connect Sine -> graph audio out", True)

    asset = editor_sub.build_to_asset(builder, "Seawall", "MS_Spike_Sine", SPIKE_PATH)
    step("build_to_asset", True, repr(asset))
except Exception:
    step("MetaSound", False, traceback.format_exc())

# --- 3. Save and report -------------------------------------------------------
try:
    unreal.EditorAssetLibrary.save_directory(SPIKE_PATH)
    listed = unreal.EditorAssetLibrary.list_assets(SPIKE_PATH)
    step("saved assets", True, list(listed))
except Exception:
    step("save", False, traceback.format_exc())

with open(REPORT, "w", encoding="utf-8") as f:
    json.dump(report, f, indent=2)
unreal.log(f"[SW-SPIKE] report written to {REPORT}")
