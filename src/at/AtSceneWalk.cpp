// Putting a scene's nodes into a space graph and taking them out: AtScene::InsertInto and RemoveFrom with the eight walks they make. See include/at/AtScene.h. In a file of its own, so that a test can supply InsertInto and RemoveFrom.
#include "at/AtScene.h"

#include "at/AtSceneActor.h"
#include "at/AtSceneNodes.h"
#include "at/AtSceneParticle.h"
#include "at/AtSpaceGraph.h"

namespace {

// The arrays of actors, in the order every walk takes them. That is not the order of the scene's fields: the level-of-detail groups come before the switch groups.
struct ActorArray {
    unsigned int AtScene::*count;
    void* AtScene::*first;
    unsigned int size;
};

const ActorArray kActorArrays[] = {
    {&AtScene::m_modelCount, &AtScene::m_models, sizeof(AtSceneModelActor)},
    {&AtScene::m_jointedModelCount, &AtScene::m_jointedModels, sizeof(AtSceneJointedModelActor)},
    {&AtScene::m_particleSystemCount, &AtScene::m_particleSystems, sizeof(AtSceneParticleSystemActor)},
    {&AtScene::m_waterCount, &AtScene::m_water, sizeof(AtSceneWaterActor)},
    {&AtScene::m_lensFlareCount, &AtScene::m_lensFlares, sizeof(AtSceneLensFlareActor)},
    {&AtScene::m_groupCount, &AtScene::m_groups, sizeof(AtSceneGroupActor)},
    {&AtScene::m_lodGroupCount, &AtScene::m_lodGroups, sizeof(AtSceneLodGroupActor)},
    {&AtScene::m_switchGroupCount, &AtScene::m_switchGroups, sizeof(AtSceneSwitchGroupActor)},
};
const unsigned int kActorArrayCount = sizeof(kActorArrays) / sizeof(kActorArrays[0]);

AtSceneActor* ActorAt(AtScene* scene, const ActorArray& array, unsigned int i) {
    return (AtSceneActor*)((char*)(scene->*array.first) + i * array.size);
}

// Whether an actor goes in: the flag asked for, no link of that kind yet, and a coordinate system of its own.
bool WantsIn(const AtSceneActor* actor, unsigned int flag, unsigned int kind) {
    return (actor->m_flags & flag) != 0 && actor->m_links[kind] == 0 && actor->m_sharedFrom == 0;
}

}  // namespace

void AtScene::InsertInto(AtSpaceGraph* graph) {
    AddDrawing(graph);
    AddCollision(graph);
    AddLineSegIntersect(graph);
    AddModification(graph);
}

void AtScene::RemoveFrom(AtSpaceGraph* graph) {
    RemoveDrawing(graph);
    RemoveCollision(graph);
    RemoveLineSegIntersect(graph);
    RemoveModification(graph);
}

void AtScene::AddDrawing(AtSpaceGraph* graph) {
    for (unsigned int a = 0; a < kActorArrayCount; a++) {
        for (unsigned int i = 0; i < this->*kActorArrays[a].count; i++) {
            AtSceneActor* actor = ActorAt(this, kActorArrays[a], i);
            if (WantsIn(actor, AtSceneActor::kDrawing, 2)) {
                graph->AddAsKind2(actor);
            }
        }
    }
}

void AtScene::RemoveDrawing(AtSpaceGraph* graph) {
    for (unsigned int a = 0; a < kActorArrayCount; a++) {
        for (unsigned int i = 0; i < this->*kActorArrays[a].count; i++) {
            AtSceneActor* actor = ActorAt(this, kActorArrays[a], i);
            if (actor->m_links[2] != 0) {
                graph->RemoveAsKind2(actor);
            }
        }
    }
}

void AtScene::AddCollision(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        AtSceneNode* node = &m_nodes[i];
        if ((node->m_flags & AtSceneNode::kCollision) != 0 && node->m_links[1] == 0) {
            graph->AddAsKind1(node);
        }
    }
    for (unsigned int a = 0; a < kActorArrayCount; a++) {
        for (unsigned int i = 0; i < this->*kActorArrays[a].count; i++) {
            AtSceneActor* actor = ActorAt(this, kActorArrays[a], i);
            if (WantsIn(actor, AtSceneNode::kCollision, 1)) {
                graph->AddAsKind1(actor);
            }
        }
    }
}

void AtScene::RemoveCollision(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        AtSceneNode* node = &m_nodes[i];
        if (node->m_links[1] != 0) {
            graph->RemoveAsKind1(node);
        }
    }
    for (unsigned int a = 0; a < kActorArrayCount; a++) {
        for (unsigned int i = 0; i < this->*kActorArrays[a].count; i++) {
            AtSceneActor* actor = ActorAt(this, kActorArrays[a], i);
            if (actor->m_links[1] != 0) {
                graph->RemoveAsKind1(actor);
            }
        }
    }
}

void AtScene::AddLineSegIntersect(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        AtSceneNode* node = &m_nodes[i];
        if ((node->m_flags & AtSceneNode::kLineSegIntersect) != 0 && node->m_links[3] == 0) {
            graph->AddAsKind3(node);
        }
    }
    for (unsigned int a = 0; a < kActorArrayCount; a++) {
        for (unsigned int i = 0; i < this->*kActorArrays[a].count; i++) {
            AtSceneActor* actor = ActorAt(this, kActorArrays[a], i);
            if (WantsIn(actor, AtSceneNode::kLineSegIntersect, 3)) {
                graph->AddAsKind3(actor);
            }
        }
    }
}

void AtScene::RemoveLineSegIntersect(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        AtSceneNode* node = &m_nodes[i];
        if (node->m_links[3] != 0) {
            graph->RemoveAsKind3(node);
        }
    }
    for (unsigned int a = 0; a < kActorArrayCount; a++) {
        for (unsigned int i = 0; i < this->*kActorArrays[a].count; i++) {
            AtSceneActor* actor = ActorAt(this, kActorArrays[a], i);
            if (actor->m_links[3] != 0) {
                graph->RemoveAsKind3(actor);
            }
        }
    }
}

void AtScene::AddModification(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_modifierNodeCount; i++) {
        AtSceneModifierNode* node = &m_modifierNodes[i];
        if ((node->m_flags & AtSceneNode::kModificationEnabled) != 0 && node->m_links[0] == 0) {
            graph->AddAsKind0(node);
        }
    }
}

void AtScene::RemoveModification(AtSpaceGraph* graph) {
    for (unsigned int i = 0; i < m_modifierNodeCount; i++) {
        AtSceneModifierNode* node = &m_modifierNodes[i];
        if (node->m_links[0] != 0) {
            graph->RemoveAsKind0(node);
        }
    }
}
