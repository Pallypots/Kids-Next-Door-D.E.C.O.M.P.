// The graphics factory's start and stop, the static factory, and the render function tables the start fills. Subsystem at/grfactory; see
// include/at/AtGraphicsFactory.h. In a file of its own so that a harness of the layer above can supply the start and stop (tests/native/test_grlayer.cpp).
// Names are inferred. Verification status per function: config/functions.d/grfactory.toml.
#include "at/AtGraphicsFactory.h"

#include "at/AtCoordSys.h"
#include "at/AtGraphicsLayer.h"
#include "at/AtMatrix.h"

AtGraphicsFactory g_atGraphicsFactory;

// ---- the start -------------------------------------------------------------------------------------------------------------

AtGraphicsFactory* AtGraphicsStartFactory(AtGraphicsGlobals* saved, const AtGraphicsConfig*) {
    saved->Restore();  // ORIGINAL: before the tables are filled, so the render table's matrix part, zero, empties the matrix table first
    AtGraphicsFillTables();
    g_atGraphicsFactory.Start();
    saved->Save();
    return &g_atGraphicsFactory;
}

void AtGraphicsStopFactory() {
    g_atGraphicsFactory.Stop();
}

// ---- the function tables ---------------------------------------------------------------------------------------------------

// In the order of the original's slots (the offset of each in the original's table beside it).
void AtMatrixTableFill(AtMatrixTable* table) {
    table->Copy = AtMatrixCopy;                                          // +0x000
    table->Identity = AtMatrixIdentity;                                  // +0x004
    table->SetTranslation = AtMatrixSetTranslation;                      // +0x00C
    table->SetScale3x3 = AtMatrixSetScale3x3;                            // +0x010
    table->SetAxisAngle = AtMatrixSetAxisAngle;                          // +0x014
    table->SetRotation = AtMatrixSetRotation;                            // +0x018
    table->SetAxes = AtMatrixSetAxes;                                    // +0x01C
    table->SetLookAt = AtMatrixSetLookAt;                                // +0x02C
    table->SetLookAtZY = AtMatrixSetLookAtZY;                            // +0x030
    table->SetLookAtXY = AtMatrixSetLookAtXY;                            // +0x034
    table->MakeTranslation = AtMatrixMakeTranslation;                    // +0x038
    table->MakeAxisAngle = AtMatrixMakeAxisAngle;                        // +0x03C
    table->MakeRotation = AtMatrixMakeRotation;                          // +0x040
    table->MakeFromAxes = AtMatrixMakeFromAxes;                          // +0x044
    table->SetUniformScale = AtMatrixSetUniformScale;                    // +0x054
    table->SetScale = AtMatrixSetScale;                                  // +0x058
    table->PreRotateX = AtMatrixPreRotateX;                              // +0x090
    table->PreRotateY = AtMatrixPreRotateY;                              // +0x094
    table->PreRotateZ = AtMatrixPreRotateZ;                              // +0x098
    table->ScaleAxes = AtMatrixScaleAxes;                                // +0x0A0
    table->TranslateAfter = AtMatrixTranslateAfter;                      // +0x0BC
    table->PostRotateZ = AtMatrixPostRotateZ;                            // +0x0D0
    table->ScaleColumnX = AtMatrixScaleColumnX;                          // +0x0DC
    table->ScaleColumnY = AtMatrixScaleColumnY;                          // +0x0E0
    table->Invert = AtMatrixInvert;                                      // +0x108
    table->SetInverse = AtMatrixSetInverse;                              // +0x10C
    table->Transpose = AtMatrixTranspose;                                // +0x118
    table->SetElement = AtMatrixSetElement;                              // +0x120
    table->GetElement = AtMatrixGetElement;                              // +0x124
    table->GetRotation = AtMatrixGetRotation;                            // +0x12C
    table->GetPosition = AtMatrixGetPosition;                            // +0x130
    table->GetAxes = AtMatrixGetAxes;                                    // +0x134
    table->GetXAxis = AtMatrixGetXAxis;                                  // +0x144
    table->GetYAxis = AtMatrixGetYAxis;                                  // +0x148
    table->GetZAxis = AtMatrixGetZAxis;                                  // +0x14C
    table->Multiply = AtMatrixMultiply;                                  // +0x160
    table->MultiplyRight = AtMatrixMultiplyRight;                        // +0x164
    table->SetProduct = AtMatrixSetProduct;                              // +0x16C
    table->TransformPoint = AtMatrixTransformPoint;                      // +0x188
    table->TransformPointZ = AtMatrixTransformPointZ;                    // +0x18C
    table->InverseTransformPoint = AtMatrixInverseTransformPoint;        // +0x190
    table->TransformDirection = AtMatrixTransformDirection;              // +0x194
    table->InverseTransformDirection = AtMatrixInverseTransformDirection;  // +0x198
    table->Read = AtMatrixRead;                                          // +0x19C
}

void AtGraphicsFillTables() {
    AtMatrixTableFill(&g_atMatrix);
    AtMatrixTableFill(&g_atRender.m_matrix);
    g_atRender.m_extended[0] = (void*)AtRenderFunction0;
    g_atRender.m_extended[1] = (void*)AtRenderFunction1;
    g_atRender.m_extended[2] = (void*)AtRenderFunction2;
    g_atRender.m_extended[3] = (void*)AtRenderFunction3;
    g_atRender.m_extended[4] = (void*)AtRenderFunction4;
    g_atRender.m_extended[5] = (void*)AtRenderFunction5;
    g_atRender.m_extended[6] = (void*)AtRenderFunction6;
}

void AtRenderFunction2() {}

