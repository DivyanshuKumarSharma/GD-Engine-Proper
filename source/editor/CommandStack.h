#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace editor
{
    // One reversible editor action. Redo() is also the "do it the first time"
    // call, so a command must be constructed in the pre-action state.
    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual void Redo() = 0;
        virtual void Undo() = 0;
        virtual std::string Name() const = 0;

        // Fold a follow-up edit of the same kind into this one (slider drags,
        // gizmo moves). Return true if `next` was absorbed.
        virtual bool MergeWith(const ICommand& next) { (void)next; return false; }
        // Merge key: only commands with equal, non-empty keys are considered.
        virtual std::string MergeKey() const { return {}; }
    };

    // Convenience command built from two lambdas.
    class FunctionCommand : public ICommand
    {
    public:
        FunctionCommand(std::string name, std::function<void()> redo, std::function<void()> undo,
                        std::string mergeKey = {})
            : m_name(std::move(name)), m_redo(std::move(redo)),
              m_undo(std::move(undo)), m_mergeKey(std::move(mergeKey)) {}

        void Redo() override { if (m_redo) m_redo(); }
        void Undo() override { if (m_undo) m_undo(); }
        std::string Name() const override { return m_name; }
        std::string MergeKey() const override { return m_mergeKey; }
        bool MergeWith(const ICommand& next) override
        {
            const auto* fn = dynamic_cast<const FunctionCommand*>(&next);
            if (!fn || m_mergeKey.empty() || fn->m_mergeKey != m_mergeKey)
            {
                return false;
            }
            // Adopt the newer "redo" so a repeated Redo lands on the latest value;
            // keep our original "undo" so one Undo returns to the pre-drag state.
            m_redo = fn->m_redo;
            return true;
        }

    private:
        std::string m_name;
        std::function<void()> m_redo;
        std::function<void()> m_undo;
        std::string m_mergeKey;
    };

    // Undo/redo history with save-point ("dirty") tracking.
    class CommandStack
    {
    public:
        // Runs cmd.Redo(), pushes it, and drops the redo branch. Consecutive
        // commands with the same non-empty MergeKey are merged instead of pushed.
        void Execute(std::unique_ptr<ICommand> command);

        // Push a command whose effect has already been applied. Same merge rules;
        // Redo() will still run on a future redo.
        void Push(std::unique_ptr<ICommand> command);

        bool CanUndo() const { return !m_undo.empty(); }
        bool CanRedo() const { return !m_redo.empty(); }
        void Undo();
        void Redo();
        void Clear();

        std::string UndoName() const { return m_undo.empty() ? std::string() : m_undo.back()->Name(); }
        std::string RedoName() const { return m_redo.empty() ? std::string() : m_redo.back()->Name(); }
        std::size_t UndoDepth() const { return m_undo.size(); }
        std::size_t RedoDepth() const { return m_redo.size(); }

        void MarkSaved();
        bool IsDirty() const;
        // Monotonic count of mutating operations since construction (tests / UI).
        long Revision() const { return m_revision; }

    private:
        std::vector<std::unique_ptr<ICommand>> m_undo;
        std::vector<std::unique_ptr<ICommand>> m_redo;
        long m_revision = 0;
        long m_savedDepth = 0;
        bool m_savedReachable = true;
    };
}
