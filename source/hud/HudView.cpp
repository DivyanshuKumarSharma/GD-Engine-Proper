#include "hud/HudView.h"

#include "course/CourseDirector.h"
#include "drills/DrillRunner.h"
#include "hud/CrosshairComponent.h"
#include "hud/HudPanelComponent.h"
#include "objectives/Objective.h"
#include "util/Format.h"
#include "util/Stopwatch.h"

#include <eng.h>

#include <string>

namespace game
{
    namespace
    {
        const glm::vec4 kWhite(1.0f, 1.0f, 1.0f, 1.0f);
        const glm::vec4 kText(0.93f, 0.95f, 0.98f, 1.0f);
        const glm::vec4 kDim(0.78f, 0.83f, 0.88f, 1.0f);
        const glm::vec4 kAmber(1.0f, 0.82f, 0.32f, 1.0f);
        const glm::vec4 kGreen(0.42f, 0.92f, 0.48f, 1.0f);
        const glm::vec4 kPanel(0.05f, 0.06f, 0.09f, 0.55f);
        const glm::vec4 kPanelStrong(0.04f, 0.05f, 0.08f, 0.72f);
        const char* kFont = "fonts/OpenSans-Regular.ttf";

        std::string ObjectivePrefix(const Objective& objective)
        {
            if (objective.IsComplete())
            {
                return "[x]  ";
            }
            if (objective.ShowsCounter())
            {
                return "[" + std::to_string(objective.CurrentAmount()) + "/" +
                    std::to_string(objective.TargetAmount()) + "]  ";
            }
            return "[ ]  ";
        }
    }

    void HudView::Init(eng::Scene* scene)
    {
        m_scene = scene;

        m_canvasObject = scene->CreateObject("HudCanvas");
        m_canvas = new eng::CanvasComponent();
        m_canvasObject->AddComponent(m_canvas);

        auto* canvasRect = new eng::RectTransformComponent();
        canvasRect->SetAnchor(glm::vec2(0.0f));
        canvasRect->SetPivot(glm::vec2(0.0f));
        m_canvasObject->AddComponent(canvasRect);

        // Panels first so they render behind the crosshair and text.
        m_panelCenter = MakePanel("HudPanelCenter");
        m_panelTopLeft = MakePanel("HudPanelTopLeft");
        m_panelTopCenter = MakePanel("HudPanelTopCenter");

        auto* crosshairObject = scene->CreateObject("HudCrosshair", m_canvasObject);
        m_crosshair = new CrosshairComponent();
        crosshairObject->AddComponent(m_crosshair);

        m_drillTitle = MakeText("HudDrillTitle", 26, glm::vec2(0.0f, 1.0f), kAmber);
        m_objectiveHeader = MakeText("HudObjHeader", 15, glm::vec2(0.0f, 1.0f), kDim);
        for (std::size_t i = 0; i < m_objectiveLines.size(); ++i)
        {
            m_objectiveLines[i] = MakeText(
                ("HudObj" + std::to_string(i)).c_str(), 18, glm::vec2(0.0f, 1.0f), kText);
        }

        m_timerLabel = MakeText("HudTimerLabel", 14, glm::vec2(0.5f, 1.0f), kDim);
        m_timerValue = MakeText("HudTimerValue", 30, glm::vec2(0.5f, 1.0f), kWhite);
        m_statsLine = MakeText("HudStats", 15, glm::vec2(1.0f, 0.0f), kDim);

        m_centerBig = MakeText("HudCenterBig", 60, glm::vec2(0.5f, 0.5f), kWhite);
        for (std::size_t i = 0; i < m_centerLines.size(); ++i)
        {
            m_centerLines[i] = MakeText(
                ("HudCenter" + std::to_string(i)).c_str(), 22, glm::vec2(0.5f, 0.5f), kText);
        }
        m_hintLine = MakeText("HudHint", 16, glm::vec2(0.5f, 0.5f), kDim);

        SetVisible(false);
    }

    eng::TextComponent* HudView::MakeText(const char* name, int fontSize,
        const glm::vec2& pivot, const glm::vec4& color)
    {
        auto* object = m_scene->CreateObject(name, m_canvasObject);

        auto* text = new eng::TextComponent();
        text->SetFont(kFont, fontSize);
        text->SetColor(color);
        object->AddComponent(text);

        auto* rect = new eng::RectTransformComponent();
        rect->SetPivot(pivot);
        object->AddComponent(rect);

        return text;
    }

    HudPanelComponent* HudView::MakePanel(const char* name)
    {
        auto* object = m_scene->CreateObject(name, m_canvasObject);
        auto* panel = new HudPanelComponent();
        object->AddComponent(panel);
        return panel;
    }

    void HudView::SetText(eng::TextComponent* text, const std::string& value, const glm::vec2& screenPos)
    {
        if (!text)
        {
            return;
        }
        text->SetText(value);
        text->GetOwner()->SetPosition2D(screenPos);
    }

    void HudView::Clear(eng::TextComponent* text)
    {
        if (text)
        {
            text->SetText(std::string());
        }
    }

    void HudView::SetVisible(bool visible)
    {
        if (m_canvas)
        {
            m_canvas->SetActive(visible);
        }
    }

    void HudView::FlashHitMarker()
    {
        if (m_crosshair)
        {
            m_crosshair->FlashHit();
        }
    }

    void HudView::Refresh(const CourseDirector& director)
    {
        const GameState state = director.State();
        if (state == GameState::MainMenu)
        {
            SetVisible(false);
            return;
        }
        SetVisible(true);

        const auto& viewport = eng::Engine::GetInstance().GetGraphicsAPI().GetViewport();
        const float width = static_cast<float>(viewport.width);
        const float height = static_cast<float>(viewport.height);

        const bool showColumns =
            state == GameState::DrillIntro || state == GameState::Countdown ||
            state == GameState::Playing || state == GameState::Paused;

        // ---- top-left: drill title + objective list ----
        const DrillDefinition* definition = director.CurrentDrillDefinition();
        const std::size_t objectiveCount = definition ? director.Runner().Objectives().size() : 0;

        if (m_panelTopLeft)
        {
            const bool show = showColumns && definition;
            m_panelTopLeft->SetVisible(show);
            m_panelTopLeft->SetColor(kPanel);
            m_panelTopLeft->SetRect(
                glm::vec2(12.0f, height - 96.0f - static_cast<float>(objectiveCount) * 26.0f),
                glm::vec2(430.0f, height - 18.0f));
        }
        if (m_panelTopCenter)
        {
            m_panelTopCenter->SetVisible(showColumns);
            m_panelTopCenter->SetColor(kPanel);
            m_panelTopCenter->SetRect(
                glm::vec2(width * 0.5f - 92.0f, height - 92.0f),
                glm::vec2(width * 0.5f + 92.0f, height - 20.0f));
        }
        if (m_panelCenter)
        {
            const bool show = state == GameState::Countdown || state == GameState::Paused ||
                state == GameState::DrillComplete || state == GameState::RunComplete;
            m_panelCenter->SetVisible(show);
            m_panelCenter->SetColor(state == GameState::Countdown ? kPanel : kPanelStrong);
            m_panelCenter->SetRect(glm::vec2(0.0f), glm::vec2(width, height));
        }

        if (showColumns && definition)
        {
            SetText(m_drillTitle, definition->name, glm::vec2(28.0f, height - 40.0f));
            SetText(m_objectiveHeader, "OBJECTIVES", glm::vec2(28.0f, height - 74.0f));

            const auto& objectives = director.Runner().Objectives();
            for (std::size_t i = 0; i < m_objectiveLines.size(); ++i)
            {
                if (i < objectives.size())
                {
                    const Objective& objective = *objectives[i];
                    SetText(m_objectiveLines[i], ObjectivePrefix(objective) + objective.Description(),
                        glm::vec2(30.0f, height - 100.0f - static_cast<float>(i) * 26.0f));
                    m_objectiveLines[i]->SetColor(objective.IsComplete() ? kGreen : kText);
                }
                else
                {
                    Clear(m_objectiveLines[i]);
                }
            }
        }
        else
        {
            Clear(m_drillTitle);
            Clear(m_objectiveHeader);
            for (auto* line : m_objectiveLines)
            {
                Clear(line);
            }
        }

        // ---- top-centre: drill timer ----
        if (showColumns)
        {
            SetText(m_timerLabel, "TIME", glm::vec2(width * 0.5f, height - 42.0f));
            SetText(m_timerValue, FormatStopwatch(director.DrillTimeSeconds()),
                glm::vec2(width * 0.5f, height - 74.0f));
        }
        else
        {
            Clear(m_timerLabel);
            Clear(m_timerValue);
        }

        // ---- bottom-right: shooting stats ----
        if (showColumns && state != GameState::DrillIntro)
        {
            const PlayerStats& stats = director.Stats();
            SetText(m_statsLine,
                "SHOTS " + std::to_string(stats.shotsFired) +
                "    HITS " + std::to_string(stats.hits) +
                "    ACC " + std::to_string(stats.AccuracyPercent()) + "%",
                glm::vec2(width - 26.0f, 24.0f));
        }
        else
        {
            Clear(m_statsLine);
        }

        m_crosshair->SetVisible(state == GameState::Playing);

        RefreshCenter(director, width, height);
    }

    void HudView::RefreshCenter(const CourseDirector& director, float width, float height)
    {
        const GameState state = director.State();
        const float cx = width * 0.5f;
        const float cy = height * 0.5f;

        Clear(m_centerBig);
        for (auto* line : m_centerLines)
        {
            Clear(line);
        }
        Clear(m_hintLine);

        auto centerLine = [&](std::size_t index, const std::string& value)
        {
            if (index < m_centerLines.size())
            {
                SetText(m_centerLines[index], value, glm::vec2(cx, cy + 12.0f - static_cast<float>(index) * 34.0f));
            }
        };
        auto bigText = [&](const std::string& value, const glm::vec4& color)
        {
            SetText(m_centerBig, value, glm::vec2(cx, cy + 84.0f));
            m_centerBig->SetColor(color);
        };
        auto hint = [&](const std::string& value)
        {
            SetText(m_hintLine, value, glm::vec2(cx, cy - 150.0f));
        };

        switch (state)
        {
        case GameState::DrillIntro:
            if (const DrillDefinition* definition = director.CurrentDrillDefinition())
            {
                bigText(definition->name, kAmber);
                centerLine(0, definition->intro);
                centerLine(1, "Par time  " + FormatStopwatch(definition->targetTimeSeconds));
            }
            break;

        case GameState::Countdown:
        {
            const int number = director.CountdownNumber();
            bigText(number > 0 ? std::to_string(number) : "GO!", number > 0 ? kWhite : kGreen);
            break;
        }

        case GameState::Paused:
            bigText("PAUSED", kAmber);
            hint("Esc  resume");
            break;

        case GameState::DrillComplete:
            bigText("DRILL COMPLETE", kAmber);
            if (const DrillResult* result = director.LastDrillResult())
            {
                centerLine(0, "TIME        " + FormatStopwatch(result->timeSeconds));
                centerLine(1, "ACCURACY    " + std::to_string(result->AccuracyPercent()) + "%");
                centerLine(2, "SCORE       " + FormatThousands(result->score));
            }
            hint("Enter  continue");
            break;

        case GameState::RunComplete:
        {
            bigText("COURSE COMPLETE", kGreen);
            const ScoreBreakdown& score = director.FinalScore();
            const HighScores& records = director.Records();
            centerLine(0, "FINAL SCORE   " + FormatThousands(score.total));
            centerLine(1, "TIME          " + FormatStopwatch(director.RunTimeSeconds()));
            centerLine(2, "ACCURACY      " + std::to_string(director.Stats().AccuracyPercent()) + "%");
            centerLine(3, "BEST  " + FormatThousands(records.highScore) +
                "    BEST TIME  " + FormatStopwatch(records.bestTimeSeconds));

            const auto recordsResult = director.FinalRecords();
            if (recordsResult.newHighScore)
            {
                centerLine(4, "NEW HIGH SCORE!");
                m_centerLines[4]->SetColor(kAmber);
            }
            else if (recordsResult.newBestTime)
            {
                centerLine(4, "NEW BEST TIME!");
                m_centerLines[4]->SetColor(kAmber);
            }
            hint("Enter  retry course     Esc  main menu");
            break;
        }

        case GameState::MainMenu:
        case GameState::Playing:
            break;
        }
    }
}
