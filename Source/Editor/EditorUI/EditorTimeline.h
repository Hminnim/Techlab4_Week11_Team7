#pragma once

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

// 트랙 데이터나 플레이어를 소유하지 않는 공통 타임라인 좌표계와 그리기 도구.
namespace EditorTimeline
{
	struct FView
	{
		float StartTime = 0.0f;
		float PixelsPerSecond = 120.0f;

		float TimeToX(float Time, float OriginX) const
		{
			return OriginX + (Time - StartTime) * PixelsPerSecond;
		}

		float XToTime(float X, float OriginX) const
		{
			return StartTime + (X - OriginX) / PixelsPerSecond;
		}

		void ZoomAt(float Factor, float MouseX, float OriginX, float MinScale, float MaxScale)
		{
			const float MouseTime = XToTime(MouseX, OriginX);
			PixelsPerSecond = std::clamp(PixelsPerSecond * Factor, MinScale, MaxScale);
			StartTime = MouseTime - (MouseX - OriginX) / PixelsPerSecond;
		}

		void PanPixels(float DeltaPixels)
		{
			StartTime += DeltaPixels / PixelsPerSecond;
		}
	};

	inline void DrawRuler(ImDrawList* Draw, const FView& View, ImVec2 Min, ImVec2 Max,
		float TrackBottom, double FPS, bool bDisplayFrames, bool bDrawGrid = false)
	{
		if (!std::isfinite(FPS) || FPS <= 0.0 || !std::isfinite(View.StartTime) ||
			!std::isfinite(View.PixelsPerSecond) || View.PixelsPerSecond <= 0.0f ||
			Max.x <= Min.x || Max.y <= Min.y) return;

		Draw->AddRectFilled(Min, Max, IM_COL32(40, 44, 48, 255));
		const double UnitsPerSecond = bDisplayFrames ? FPS : 1.0;
		const double MinStep = bDisplayFrames ? 1.0 : 1.0 / FPS;
		const double RawStep = std::max(MinStep, 60.0 * UnitsPerSecond / View.PixelsPerSecond);
		const double Magnitude = std::pow(10.0, std::floor(std::log10(RawStep)));
		const double Normalized = RawStep / Magnitude;
		const double Step = Magnitude * (Normalized <= 1.0 ? 1.0 : Normalized <= 2.0 ? 2.0 : Normalized <= 5.0 ? 5.0 : 10.0);
		const double MinorStep = bDisplayFrames ? std::max(1.0, std::floor(Step / 5.0)) : Step / 5.0;
		const double FirstUnit = std::ceil(View.StartTime * UnitsPerSecond / MinorStep) * MinorStep;
		const double LastUnit = View.XToTime(Max.x, Min.x) * UnitsPerSecond;

		Draw->PushClipRect(Min, ImVec2(Max.x, std::max(Max.y, TrackBottom)), true);
		// 눈금 수는 화면 폭과 간격으로 제한하고 큰 시간 값의 덧셈 누적을 피한다.
		const int TickCount = static_cast<int>(std::ceil((LastUnit - FirstUnit) / MinorStep)) + 1;
		for (int Index = 0; Index < TickCount; ++Index)
		{
			const double Unit = FirstUnit + Index * MinorStep;
			const float X = View.TimeToX(static_cast<float>(Unit / UnitsPerSecond), Min.x);
			const bool bMajor = std::abs(Unit - std::round(Unit / Step) * Step) < MinorStep * 0.001;
			Draw->AddLine({ X, Max.y - (bMajor ? 7.0f : 3.0f) }, { X, Max.y }, IM_COL32(150, 150, 150, 255));
			if (!bMajor) continue;
			if (bDrawGrid)
				Draw->AddLine({ X, Max.y }, { X, TrackBottom }, IM_COL32(55, 58, 62, 255));

			char Label[32];
			std::snprintf(Label, sizeof(Label), bDisplayFrames ? "%.0f" : "%.2f", Unit);
			Draw->AddText({ X + 3.0f, Min.y + 3.0f }, IM_COL32(205, 205, 205, 255), Label);
		}
		Draw->PopClipRect();
	}

	inline void DrawPlayhead(ImDrawList* Draw, const FView& View, float CurrentTime,
		ImVec2 Min, ImVec2 Max, ImU32 Color = IM_COL32(230, 80, 60, 255), bool bDrawHandle = true)
	{
		const float X = View.TimeToX(CurrentTime, Min.x);
		if (!std::isfinite(X) || X < Min.x || X > Max.x || Max.y <= Min.y) return;

		Draw->PushClipRect(Min, Max, true);
		const float LineX = std::min(X, Max.x - 1.0f);
		Draw->AddLine({ LineX, Min.y }, { LineX, Max.y }, Color, 2.0f);
		if (bDrawHandle)
			Draw->AddTriangleFilled({ X - 5.0f, Min.y }, { X + 5.0f, Min.y }, { X, Min.y + 8.0f }, Color);
		Draw->PopClipRect();
	}
}
