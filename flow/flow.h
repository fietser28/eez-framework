/*
* EEZ Generic Firmware
* Copyright (C) 2021-present, Envox d.o.o.
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

#pragma once

#include <eez/gui/assets.h>

namespace eez {
namespace flow {

using namespace eez::gui;

struct FlowState;

unsigned start(eez::gui::Assets *assets);
void tick();
void stop();

FlowState *getFlowState(Assets *assets, int flowStateIndex);

FlowState *getFlowState(Assets *assets, int16_t pageId, const WidgetCursor &widgetCursor);
int getPageIndex(FlowState *flowState);

FlowState *getLayoutViewFlowState(FlowState *flowState, uint16_t layoutViewWidgetComponentIndex, int16_t pageId);

void executeFlowAction(const WidgetCursor &widgetCursor, int16_t actionId);
void dataOperation(int16_t dataId, DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value);
int16_t getNativeVariableId(const WidgetCursor &widgetCursor);

void onDebuggerClientConnected();
void onDebuggerClientDisconnected();

void executeScpi();
void flushToDebuggerMessage();

#if defined(__EMSCRIPTEN__)
struct DashboardComponentContext {
    FlowState *flowState;
    unsigned componentIndex;

    int getFlowStateIndex();
    int getFlowIndex();
    int getComponentIndex();

    DashboardComponentContext *startAsyncExecution();
    void endAsyncExecution();

    Value *evalProperty(int propertyIndex);

    const char *getStringParam(int offset);

    void *getExpressionListParam(int offset);
    void freeExpressionListParam(void *ptr);

    void propagateValue(unsigned outputIndex, Value *valuePtr);
    void propagateIntValue(unsigned outputIndex, int value);
    void propagateDoubleValue(unsigned outputIndex, double value);
    void propagateBooleanValue(unsigned outputIndex, bool value);
    void propagateStringValue(unsigned outputIndex, const char *value);
    void propagateUndefinedValue(unsigned outputIndex);
    void propagateNullValue(unsigned outputIndex);

    void propagateValueThroughSeqout();

    void executeCallAction(int flowIndex);

    void throwError(const char *errorMessage);
};
#endif // __EMSCRIPTEN__

} // flow
} // eez
