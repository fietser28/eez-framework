/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <math.h>
#include <float.h>

#include <eez/core/alloc.h>
#include <eez/core/util.h>

#include <eez/gui/gui.h>
#include <eez/gui/data.h>
#include <eez/gui/widgets/line_chart.h>

#include <eez/flow/components/line_chart_widget.h>

namespace eez {
namespace gui {

enum AxisPosition {
    AXIS_POSITION_X,
    AXIS_POSITION_Y,
};

enum AxisValueType {
    AXIS_VALUE_TYPE_NUMBER,
    AXIS_VALUE_TYPE_DATE,
};

struct Axis {
    AxisPosition position;
    AxisValueType valueType;
    Rect rect;
    int maxTicks;
    double min;
    double max;
    double offset;
    double scale;
    double ticksDelta;
};

struct Chart {
    Axis xAxis;
    Axis yAxis;
};

void calcAutoTicks(Axis &axis) {
    auto pxStart =
        axis.position == AXIS_POSITION_X
            ? axis.rect.x
            : axis.rect.y + axis.rect.h;
    auto pxRange =
        axis.position == AXIS_POSITION_X
            ? axis.rect.w
            : -axis.rect.h;

    auto range = axis.max - axis.min;

    auto min = axis.min - 0.1 * range;
    auto max = axis.max + 0.1 * range;

    range = max - min;

    axis.scale = pxRange / range;
    axis.offset = pxStart - min * axis.scale;

    auto x = range / axis.maxTicks;
    auto exp = floor(log10(x));
    auto nx = x * pow(10, -exp);
    auto ndelta = nx < 2 ? 2 : nx < 5 ? 5 : 10;
    auto delta = ndelta * pow(10, exp);
    axis.ticksDelta = delta;
}

////////////////////////////////////////////////////////////////////////////////

bool LineChartWidgetState::updateState() {
    WIDGET_STATE_START(LineChartWidget);

    WIDGET_STATE(flags.active, g_isActiveWidget);

    WIDGET_STATE(title, get(widgetCursor, widget->title));

    WIDGET_STATE(showLegendValue, get(widgetCursor, widget->showLegend));

    if (widget->yAxisRangeOption == Y_AXIS_RANGE_OPTION_FIXED) {
        WIDGET_STATE(yAxisRangeFrom, get(widgetCursor, widget->yAxisRangeFrom));
        WIDGET_STATE(yAxisRangeTo, get(widgetCursor, widget->yAxisRangeTo));
    }

	if (widgetCursor.flowState) {
        auto executionState = (flow::LineChartWidgetComponenentExecutionState *)widgetCursor.flowState->componenentExecutionStates[widget->componentIndex];
        if (executionState->updated) {
            executionState->updated = false;
            hasPreviousState = false;
        }
    }

    WIDGET_STATE_END()
}

void LineChartWidgetState::render() {
    using namespace display;

    const WidgetCursor &widgetCursor = g_widgetCursor;
	auto widget = (const LineChartWidget*)widgetCursor.widget;

	if (!widgetCursor.flowState) {
        return;
    }

    auto executionState = (flow::LineChartWidgetComponenentExecutionState *)widgetCursor.flowState->componenentExecutionStates[widget->componentIndex];
    if (!executionState) {
        return;
    }

    auto component = (flow::LineChartWidgetComponenent *)widgetCursor.flowState->flow->components[widget->componentIndex];

	const Style* style = getStyle(widget->style);
	const Style* legendStyle = getStyle(widget->legendStyle);
	const Style* xAxisStyle = getStyle(widget->xAxisStyle);
	const Style* yAxisStyle = getStyle(widget->yAxisStyle);

    auto isActive = flags.active;

    auto showLegend = showLegendValue.toBool(nullptr);

    Chart chart;

    chart.xAxis.position = AXIS_POSITION_X;
    chart.xAxis.valueType = AXIS_VALUE_TYPE_NUMBER;

    chart.yAxis.position = AXIS_POSITION_Y;
    chart.yAxis.valueType = AXIS_VALUE_TYPE_NUMBER;

    if (executionState->numPoints > 0) {
        chart.xAxis.min = FLT_MAX;
        chart.xAxis.max = -FLT_MAX;

        chart.yAxis.min = FLT_MAX;
        chart.yAxis.max = -FLT_MAX;

        for (uint32_t i = 0; i < executionState->numPoints; i++) {
            uint32_t pointIndex = (executionState->startPointIndex + i) % executionState->maxPoints;

            Value xValue = executionState->getX(pointIndex);
            auto x = xValue.toDouble(nullptr);
            if (x < chart.xAxis.min) chart.xAxis.min = x;
            if (x > chart.xAxis.max) chart.xAxis.max = x;
            if (i == 0) {
                chart.xAxis.valueType = xValue.getType() == VALUE_TYPE_DATE ? AXIS_VALUE_TYPE_DATE : AXIS_VALUE_TYPE_NUMBER;
            }

            if (widget->yAxisRangeOption == Y_AXIS_RANGE_OPTION_FLOATING) {
                for (uint32_t lineIndex = 0; lineIndex < executionState->numLines; lineIndex++) {
                    auto y = executionState->getY(pointIndex, lineIndex);
                    if (y < chart.yAxis.min) chart.yAxis.min = y;
                    if (y > chart.yAxis.max) chart.yAxis.max = y;
                }
            }
        }

        if (widget->yAxisRangeOption == Y_AXIS_RANGE_OPTION_FIXED) {
            chart.yAxis.min = yAxisRangeFrom.toDouble();
            chart.yAxis.max = yAxisRangeTo.toDouble();
        }
    } else {
        chart.xAxis.min = 0;
        chart.xAxis.max = 0;
        chart.yAxis.min = 0;
        chart.yAxis.max = 0;
    }

    if (chart.xAxis.min >= chart.xAxis.max) {
        chart.xAxis.min = 0;
        chart.xAxis.max = 1;
    }

    if (chart.yAxis.min >= chart.yAxis.max) {
        chart.yAxis.min = 0;
        chart.yAxis.max = 1;
    }

    Rect widgetRect = {
        0,
        0,
        widgetCursor.w,
        widgetCursor.h
    };

    int16_t marginLeft = widget->marginLeft;
    int16_t marginTop = widget->marginTop;
    int16_t marginRight = widget->marginRight;
    int16_t marginBottom = widget->marginBottom;

    // measure legend width
    static const unsigned LEGEND_ICON_WIDTH = 32;
    int legendWidth;
    int legendLineHeight;
    if (showLegend) {
        auto legendFont = styleGetFont(legendStyle);

        unsigned maxWidth = 0;

        for (uint32_t lineIndex = 0; lineIndex < executionState->numLines; lineIndex++) {
            static const size_t MAX_LINE_LABEL_LEN = 128;
            char text[MAX_LINE_LABEL_LEN + 1];
            executionState->lineLabels[lineIndex].toText(text, sizeof(text));
            if (*text == 0) {
                snprintf(text, sizeof(text), "Trace %d", lineIndex + 1);
            }

            auto width = measureStr(
                text, -1,
                legendFont,
                widgetRect.w - LEGEND_ICON_WIDTH
            );
            if (width > maxWidth) {
                maxWidth = width;
            }
        }

        legendWidth = LEGEND_ICON_WIDTH + maxWidth,
        legendLineHeight = legendFont.getHeight();
    } else {
        legendWidth = 0;
    }

    if (legendWidth > marginRight) {
        marginRight = legendWidth;
    }

    Rect gridRect;
    gridRect.x = widgetRect.x + marginLeft;
    gridRect.y = widgetRect.y + marginTop;
    gridRect.w = widgetRect.w - (marginLeft + marginRight);
    gridRect.h = widgetRect.h - (marginTop + marginBottom);

    chart.xAxis.maxTicks = chart.xAxis.valueType == AXIS_VALUE_TYPE_DATE ? 4 : 8;

    chart.xAxis.rect.x = gridRect.x;
    chart.xAxis.rect.y = gridRect.y + gridRect.h;
    chart.xAxis.rect.w = gridRect.w;
    chart.xAxis.rect.h = marginBottom;

    chart.yAxis.maxTicks = 8;

    chart.yAxis.rect.x = widgetRect.x;
    chart.yAxis.rect.y = gridRect.y;
    chart.yAxis.rect.w = marginLeft;
    chart.yAxis.rect.h = gridRect.h;

    calcAutoTicks(chart.xAxis);
    calcAutoTicks(chart.yAxis);

	// init AGG
	display::AggDrawing aggDrawing;
	display::aggInit(aggDrawing);
	auto &graphics = aggDrawing.graphics;

	graphics.clipBox(widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w, widgetCursor.y + widgetCursor.h);
	graphics.translate(widgetCursor.x, widgetCursor.y);

	// clear background
	setColor(isActive ? style->activeBackgroundColor : style->backgroundColor);
	fillRect(widgetCursor.x, widgetCursor.y, widgetCursor.x + widgetCursor.w - 1, widgetCursor.y + widgetCursor.h - 1);

    // draw title
    static const size_t MAX_TITLE_LEN = 128;
    char text[MAX_TITLE_LEN + 1];
    title.toText(text, sizeof(text));
    drawText(
        text, -1,
        widgetCursor.x, widgetCursor.y, widgetCursor.w, marginTop,
        style,
        flags.active
    );

    // draw legend
    if (showLegend) {
        auto x = widgetCursor.x + gridRect.x + gridRect.w;
        auto y = widgetCursor.y + gridRect.y;
        auto w = marginRight;
        auto h = gridRect.h;

        x = x + w - legendWidth;

        for (uint32_t lineIndex = 0; lineIndex < executionState->numLines; lineIndex++) {
            if (
                y + legendLineHeight >
                gridRect.y + gridRect.h
            ) {
                break;
            }

            auto color = component->lines[lineIndex]->color;
            setColor(component->lines[lineIndex]->color);

            fillRect(
                x,
                y + (legendLineHeight - 2) / 2,
                x + LEGEND_ICON_WIDTH - 4,
                y + (legendLineHeight - 2) / 2 + 2
            );

            auto color16 = getColor16FromIndex(color);
            graphics.fillColor(COLOR_TO_R(color16), COLOR_TO_G(color16), COLOR_TO_B(color16));
            graphics.noLine();
            graphics.ellipse(
                -widgetCursor.x + x + (LEGEND_ICON_WIDTH - 4) / 2,
                -widgetCursor.y + y + legendLineHeight / 2,
                3, 3
            );

            static const size_t MAX_LINE_LABEL_LEN = 128;
            char text[MAX_LINE_LABEL_LEN + 1];
            executionState->lineLabels[lineIndex].toText(text, sizeof(text));
            if (*text == 0) {
                snprintf(text, sizeof(text), "Trace %d", lineIndex + 1);
            }
            drawText(
                text,
                -1,
                x + LEGEND_ICON_WIDTH,
                y,
                legendWidth - LEGEND_ICON_WIDTH,
                legendLineHeight,
                legendStyle,
                false
            );

            y += legendLineHeight;
        }
    }

    // draw X axis
    {
        auto from = ceil(chart.xAxis.min / chart.xAxis.ticksDelta) * chart.xAxis.ticksDelta;
        auto to = floor(chart.xAxis.max / chart.xAxis.ticksDelta) * chart.xAxis.ticksDelta;

        auto w = chart.xAxis.ticksDelta * chart.xAxis.scale;

        auto &axis = chart.xAxis;
        auto &rect = axis.rect;

        for (unsigned i = 0; i < axis.maxTicks; i++) {
            auto tick = from + i * axis.ticksDelta;
            if (tick > to) break;

            auto x = axis.offset + tick * axis.scale;

            char textBuffer[128];

            char *text;
            int textLength;

            char *textDate = nullptr;
            int textDateLength;

            if (axis.valueType == AXIS_VALUE_TYPE_NUMBER) {
                snprintf(textBuffer, sizeof(textBuffer), "%g", tick);
                text = textBuffer;
                textLength = -1;
            } else {
                Value value(tick, VALUE_TYPE_DATE);
                value.toText(textBuffer, sizeof(textBuffer));

                // date format: YYYY-MM-DD HH:MM:SS.000000

                // remove trailing zeros from time
                for (auto i = strlen(textBuffer) - 1; i >= 0; i--) {
                    if (textBuffer[i] == '0') {
                        textBuffer[i] = 0;
                    } else {
                        if (textBuffer[i] == '.') {
                            textBuffer[i] = 0;
                        }
                        break;
                    }
                }

                // get time part
                text = strchr(textBuffer, ' ') + 1;
                textLength = -1;

                // get date part
                textDate = textBuffer;
                textDateLength = text - textBuffer - 1;
            }

            auto xText = widgetCursor.x + (int)round(x - w / 2);
            auto wText = (int)round(w);
            if (xText < rect.x) {
                xText = rect.x;
            }
            if (xText + wText > rect.x + rect.w) {
                wText = rect.x + rect.w - xText;
            }

            drawText(
                text, textLength,
                xText,
                widgetCursor.y + rect.y,
                wText,
                rect.h / (textDate ? 2 : 1),
                xAxisStyle,
                flags.active
            );

            if (i == 0 && textDate) {
                drawText(
                    textDate, textDateLength,
                    xText,
                    widgetCursor.y + rect.y + rect.h / 2,
                    wText,
                    rect.h / 2,
                    xAxisStyle,
                    flags.active
                );
            }
        }
    }

    // draw Y axis
    {
        auto from = ceil(chart.yAxis.min / chart.yAxis.ticksDelta) * chart.yAxis.ticksDelta;
        auto to = floor(chart.yAxis.max / chart.yAxis.ticksDelta) * chart.yAxis.ticksDelta;

        auto h = abs(chart.yAxis.ticksDelta * chart.yAxis.scale);

        auto &axis = chart.yAxis;
        auto &rect = axis.rect;

        for (unsigned i = 0; i < axis.maxTicks; i++) {
            auto tick = from + i * axis.ticksDelta;
            if (tick > to) break;

            auto y = axis.offset + tick * axis.scale;

            char text[128];
            snprintf(text, sizeof(text), "%g", tick);

            auto yText = widgetCursor.y + (int)roundf(y - h / 2);
            auto hText = (int)round(h);
            if (yText < rect.y) {
                yText = rect.y;
            }
            if (yText + hText > rect.y + rect.h) {
                hText = rect.y + rect.h - yText;
            }

            drawText(
                text, -1,
                widgetCursor.x + rect.x,
                yText,
                rect.w,
                hText,
                yAxisStyle,
                flags.active
            );
        }
    }

    // draw grid
    setColor(style->borderColor);

    {
        // vertical lines
        auto from = ceil(chart.xAxis.min / chart.xAxis.ticksDelta) * chart.xAxis.ticksDelta;
        auto to = floor(chart.xAxis.max / chart.xAxis.ticksDelta) * chart.xAxis.ticksDelta;
        for (unsigned i = 0; ; i++) {
            auto tick = from + i * chart.xAxis.ticksDelta;
            if (tick > to) break;
            auto x = chart.xAxis.offset + tick * chart.xAxis.scale;

            drawVLine(widgetCursor.x + (int)round(x), widgetCursor.y + gridRect.y, gridRect.h);
        }
    }

    {
        // horizontal lines
        auto from = ceil(chart.yAxis.min / chart.yAxis.ticksDelta) * chart.yAxis.ticksDelta;
        auto to = floor(chart.yAxis.max / chart.yAxis.ticksDelta) * chart.yAxis.ticksDelta;
        for (unsigned i = 0; ; i++) {
            auto tick = from + i * chart.yAxis.ticksDelta;
            if (tick > to) break;
            auto y = chart.yAxis.offset + tick * chart.yAxis.scale;

            drawHLine(widgetCursor.x + gridRect.x, widgetCursor.y + (int)round(y), gridRect.w);
        }
    }

    // draw lines
    {
        graphics.clipBox(widgetCursor.x + gridRect.x, widgetCursor.x + gridRect.y, widgetCursor.x + gridRect.x + gridRect.w, widgetCursor.x + gridRect.y + gridRect.h);

        for (uint32_t lineIndex = 0; lineIndex < executionState->numLines; lineIndex++) {
            graphics.resetPath();

            for (uint32_t i = 0; i < executionState->numPoints; i++) {
                uint32_t pointIndex = (executionState->startPointIndex + i) % executionState->maxPoints;

                Value xValue = executionState->getX(pointIndex);
                auto x = chart.xAxis.offset + xValue.toDouble(nullptr) * chart.xAxis.scale;

                auto y = chart.yAxis.offset + executionState->getY(pointIndex, lineIndex) * chart.yAxis.scale;

                if (i == 0) {
                    graphics.moveTo(x, y);
                } else {
                    graphics.lineTo(x, y);
                }
            }

            auto color16 = getColor16FromIndex(component->lines[lineIndex]->color);
            graphics.lineColor(COLOR_TO_R(color16), COLOR_TO_G(color16), COLOR_TO_B(color16));
            graphics.lineWidth(1.5);
            graphics.noFill();
            graphics.drawPath();
        }
    }
}

} // namespace gui
} // namespace eez