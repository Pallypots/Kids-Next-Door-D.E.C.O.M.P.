# Mutants of what the sound manager needs of the Atlas classes (src/at/AtAudioVoices.cpp). The same oracle as the gmsound suite. Each text occurs exactly once in the source.
F = "AtAudioVoices.cpp"

# Not mutated: the loader's slots 4 to 7 here are diagnostics for a real build; the oracle supplies its own Wait.
MUTANTS = [
    ("is playing: which", F, "if (voice->m_segment == segment) {", "if (voice->m_segment != segment) {"),
    ("is playing: returns", F, "            return voice;\n        }\n    }\n    return 0;", "            return voice->m_next;\n        }\n    }\n    return 0;"),
    ("is playing: from", F, "for (AtAudioVoice* voice = (AtAudioVoice*)m_firstVoice; voice != 0; voice = voice->m_next) {", "for (AtAudioVoice* voice = (AtAudioVoice*)m_lastVoice; voice != 0; voice = voice->m_next) {"),
    ("first voice", F, "AtAudioVoice* AtAudioEmitter::FirstVoice() {\n    return (AtAudioVoice*)m_firstVoice;", "AtAudioVoice* AtAudioEmitter::FirstVoice() {\n    return (AtAudioVoice*)m_lastVoice;"),
    ("next voice", F, "    return voice->m_next;\n}", "    return voice->m_next ? voice->m_next->m_next : 0;\n}"),
    ("mark: which", F, "for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {", "for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_firstChild) {"),
    ("mark: how", F, "child->m_dirty |= mask;", "child->m_dirty = mask;"),
    ("local: children", F, "    MarkChildrenDirty(kAllLocal);\n", ""),
    ("local: mask", F, "    m_dirty |= kAllLocal;\n", "    m_dirty |= kWorld;\n"),
    ("local: world bit", F, "m_dirty &= ~(unsigned int)kWorld;", "m_dirty &= ~(unsigned int)kInverse;"),
    ("local: copy", F, "g_atMatrix.Copy(m_local, m_world);", "g_atMatrix.Copy(m_world, m_local);"),
    ("local: parent inverse", F, "    m_parent->UpdateInverse();\n", ""),
    ("local: product", F, "g_atMatrix.SetProduct(m_local, m_world, m_parent->m_world);", "g_atMatrix.SetProduct(m_local, m_world, m_parent->m_local);"),
    ("local: parent test", F, "    if (m_parent == 0) {\n        m_dirty &=", "    if (m_parent == 0 || m_firstChild != 0) {\n        m_dirty &="),
    ("set world: axis", F, "g_atMatrix.MakeAxisAngle(m_world, axis, angle);", "g_atMatrix.MakeAxisAngle(m_local, axis, angle);"),
    ("set world: translation", F, "g_atMatrix.SetTranslation(m_world, position);", "g_atMatrix.SetTranslation(m_world, axis);"),
    ("set world: with", F, "    if (with != 0) {\n        g_atMatrix.ScaleAxes(m_world, with);\n    }\n", "    g_atMatrix.ScaleAxes(m_world, with);\n"),
    ("set world: local", F, "        g_atMatrix.ScaleAxes(m_world, with);\n    }\n    LocalFromWorld();", "        g_atMatrix.ScaleAxes(m_world, with);\n    }"),
    ("position: root", F, "        g_atMatrix.GetPosition(m_local, out);", "        g_atMatrix.GetPosition(m_world, out);"),
    ("position: update", F, "    UpdateWorld();\n    g_atMatrix.GetPosition(m_world, out);", "    g_atMatrix.GetPosition(m_world, out);"),
    ("position: child", F, "    UpdateWorld();\n    g_atMatrix.GetPosition(m_world, out);", "    UpdateWorld();\n    g_atMatrix.GetPosition(m_local, out);"),
    ("node position: coords", F, "    if (m_coordSys != 0) {\n        m_coordSys->WorldPosition(out);\n        return;", "    if (m_coordSys != 0) {\n        m_coordSys->WorldPosition(out);"),
    ("node position: zero", F, "    out->y = 0;\n", "    out->y = 1;\n"),
]

SUITE = {"target": "oracle_gmsound", "subsystem": "at/gmsound", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
