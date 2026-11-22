#include "UserScripts.h"
#include "Engine/Scene/ScriptRegistry.h"
#include "Engine/Core/Log.h"

namespace Overflow
{
    // -------------------------------------------------------------------------
    // Overflow Engine - Centralized Script Registry
    // Todos os scripts de gameplay (OBehaviour) ficam 100% limpos,
    // sem boilerplate, structs ou namespaces extras nos arquivos do usuario.
    // -------------------------------------------------------------------------
    void ForceLinkUserScripts()
    {
#if __has_include("../../../assets/scripts/MyBehavior.h")
        ScriptRegistry::Register<MyBehavior>("MyBehavior", "Comportamento com forca vertical");
#endif

#if __has_include("../../../assets/scripts/MyScriptTest.h")
        ScriptRegistry::Register<MyScriptTest>("MyScriptTest", "Script de teste");
#endif

        OVERFLOW_INFO("User scripts (OBehaviour) vinculados com sucesso.");
    }
}
