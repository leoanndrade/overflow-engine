#pragma once

#include "Timestep.h"
#include <string>

namespace Overflow {

    class Layer {
    public:
        Layer(const std::string& name = "Layer") : m_DebugName(name) {}
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(Timestep) {}
        virtual void OnImGuiRender() {}

        inline const std::string& GetName() const { return m_DebugName; }

    protected:
        std::string m_DebugName;
    };

}
