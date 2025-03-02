/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.EventBreakpointTreeController = class EventBreakpointTreeController
{
    constructor(treeOutline)
    {
        this._treeOutline = treeOutline;

        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.EventBreakpointAdded, this._eventBreakpointAdded, this);
        WI.domDebuggerManager.addEventListener(WI.DOMDebuggerManager.Event.EventBreakpointRemoved, this._eventBreakpointRemoved, this);

        for (let breakpoint of WI.domTreeManager.eventListenerBreakpoints)
            this._addTreeElement(breakpoint);

        for (let breakpoint of WI.domDebuggerManager.eventBreakpoints)
            this._addTreeElement(breakpoint);
    }

    // Public

    revealAndSelect(breakpoint)
    {
        let treeElement = this._treeOutline.findTreeElement(breakpoint);
        if (!treeElement)
            return;

        treeElement.revealAndSelect();
    }

    // Private

    _eventBreakpointAdded(event)
    {
        this._addTreeElement(event.data.breakpoint);
    }

    _eventBreakpointRemoved(event)
    {
        let breakpoint = event.data.breakpoint;
        let treeElement = this._treeOutline.findTreeElement(breakpoint);
        if (!treeElement)
            return;

        this._treeOutline.removeChild(treeElement);
    }

    _addTreeElement(breakpoint)
    {
        let treeElement = this._treeOutline.findTreeElement(breakpoint);
        console.assert(!treeElement);
        if (treeElement)
            return;

        this._treeOutline.appendChild(new WI.EventBreakpointTreeElement(breakpoint, {
            linkifyNode: true,
        }));
    }
};
