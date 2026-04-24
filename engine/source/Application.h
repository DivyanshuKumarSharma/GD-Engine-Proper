#pragma once

namespace eng{
    class Application
    {
    public:
        virtual bool Init() = 0;
        //deltatime is in seconds
        virtual void Update(float deltaTime) = 0;
        virtual void Destroy() = 0;

        void setNeedsToBeClosed(bool value);
        bool needsToBeClosed() const;

    private:
        bool m_needsToBeClosed = false;
    };
}