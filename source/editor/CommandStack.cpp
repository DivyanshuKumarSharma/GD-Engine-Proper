#include "editor/CommandStack.h"

namespace editor
{
    void CommandStack::Execute(std::unique_ptr<ICommand> command)
    {
        if (!command)
        {
            return;
        }
        command->Redo();
        Push(std::move(command));
    }

    void CommandStack::Push(std::unique_ptr<ICommand> command)
    {
        if (!command)
        {
            return;
        }

        const bool hadRedoBranch = !m_redo.empty();
        m_redo.clear();
        ++m_revision;

        // If the save point was ahead of us on a branch we just cut, it can never
        // be reached again.
        if (hadRedoBranch && m_savedDepth > static_cast<long>(m_undo.size()))
        {
            m_savedReachable = false;
        }

        const std::string key = command->MergeKey();
        if (!key.empty() && !m_undo.empty() && m_undo.back()->MergeKey() == key &&
            m_undo.back()->MergeWith(*command))
        {
            return; // merged; undo depth unchanged
        }

        m_undo.push_back(std::move(command));
    }

    void CommandStack::Undo()
    {
        if (m_undo.empty())
        {
            return;
        }
        m_undo.back()->Undo();
        m_redo.push_back(std::move(m_undo.back()));
        m_undo.pop_back();
        ++m_revision;
    }

    void CommandStack::Redo()
    {
        if (m_redo.empty())
        {
            return;
        }
        m_redo.back()->Redo();
        m_undo.push_back(std::move(m_redo.back()));
        m_redo.pop_back();
        ++m_revision;
    }

    void CommandStack::Clear()
    {
        m_undo.clear();
        m_redo.clear();
        m_revision = 0;
        m_savedDepth = 0;
        m_savedReachable = true;
    }

    void CommandStack::MarkSaved()
    {
        m_savedDepth = static_cast<long>(m_undo.size());
        m_savedReachable = true;
    }

    bool CommandStack::IsDirty() const
    {
        if (!m_savedReachable)
        {
            return true;
        }
        return static_cast<long>(m_undo.size()) != m_savedDepth;
    }
}
