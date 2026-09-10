#pragma once

namespace eng
{
    class Application
    {
    public:
        virtual ~Application() = default;
        virtual void RegisterTypes();
        virtual bool Init() = 0;
        // deltaTime in seconds
        virtual void Update(float deltaTime) = 0;
        virtual void Destroy() = 0;

        // Called once per frame after the scene has been drawn but before the
        // backbuffer is presented. Tools use it to render an editor overlay on
        // top of (or instead of) the scene. Default: no-op.
        virtual void OnPostRender() {}

        void SetNeedsToBeClosed(bool value);
        bool NeedsToBeClosed() const;

    private:
        bool m_needsToBeClosed = false;
    };
}