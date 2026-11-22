#pragma once

#include "Entity.h"
#include "Engine/Core/Timestep.h"

namespace Overflow {

    // -------------------------------------------------------------------------
    // Overflow Engine: EntityBehavior / ScriptableEntity
    // Arquitetura própria de lógica de gameplay e inteligência para entidades.
    // Une a performance pura do C++ nativo com um ciclo de vida limpo e intuitivo.
    // -------------------------------------------------------------------------
    class ScriptableEntity {
    public:
        virtual ~ScriptableEntity() = default;

        template<typename T>
        T& GetComponent() {
            return m_Entity.GetComponent<T>();
        }

        template<typename T>
        const T& GetComponent() const {
            return m_Entity.GetComponent<T>();
        }

        template<typename T>
        bool HasComponent() const {
            return m_Entity.HasComponent<T>();
        }

        Entity GetEntity() const { return m_Entity; }
        Scene* GetScene() const { return m_Entity.GetScene(); }

    protected:
        // ---------------------------------------------------------------------
        // Ciclo de Vida Oficial da Overflow Engine (Opcao 2)
        // ---------------------------------------------------------------------
        virtual void OnInit() {}
        virtual void OnEnable() {}
        virtual void OnTick(Timestep ts) { OnUpdate(ts); }
        virtual void OnDisable() {}
        virtual void OnDestroy() {}

        // Metodos de retrocompatibilidade
        virtual void OnCreate() {}
        virtual void OnUpdate(Timestep /*ts*/) {}

    private:
        Entity m_Entity;
        friend class Scene;
    };

    // -------------------------------------------------------------------------
    // Overflow Engine: OBehaviour (Identidade oficial de Gameplay da Overflow)
    // -------------------------------------------------------------------------
    using OBehaviour = ScriptableEntity;
    using EntityBehavior = ScriptableEntity;
    using OvBehavior = ScriptableEntity;
    using OverflowBehavior = ScriptableEntity;

}
