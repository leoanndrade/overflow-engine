#pragma once

#include "Engine/Core/Layer.h"

namespace Overflow {

    class ImGuiLayer : public Layer {
    public:
        ImGuiLayer();
        virtual ~ImGuiLayer() override = default;

        virtual void OnAttach() override;
        virtual void OnDetach() override;

        void Begin();
        void End();

        void SetDarkThemeColors();

    private:
        float m_Time = 0.0f;
    };

}
