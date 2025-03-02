/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "config.h"
#include "TextChecker.h"

#include "TextCheckerState.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {
using namespace WebCore;

static TextCheckerState textCheckerState;

const TextCheckerState& TextChecker::state()
{
    return textCheckerState;
}

void TextChecker::setTestingMode(bool)
{
}

bool TextChecker::isTestingMode()
{
    notImplemented();
    return false;
}

bool TextChecker::isContinuousSpellCheckingAllowed()
{
    notImplemented();
    return false;
}

void TextChecker::setContinuousSpellCheckingEnabled(bool)
{
    notImplemented();
}

void TextChecker::setGrammarCheckingEnabled(bool)
{
    notImplemented();
}

void TextChecker::continuousSpellCheckingEnabledStateChanged(bool)
{
    notImplemented();
}

void TextChecker::grammarCheckingEnabledStateChanged(bool)
{
    notImplemented();
}

int64_t TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
    notImplemented();
    return false;
}

void TextChecker::closeSpellDocumentWithTag(int64_t)
{
    notImplemented();
}

void TextChecker::checkSpellingOfString(int64_t, StringView, int32_t&, int32_t&)
{
    notImplemented();
}

void TextChecker::checkGrammarOfString(int64_t, StringView, Vector<WebCore::GrammarDetail>&, int32_t&, int32_t&)
{
    notImplemented();
}

bool TextChecker::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void TextChecker::toggleSpellingUIIsShowing()
{
    notImplemented();
}

void TextChecker::updateSpellingUIWithMisspelledWord(int64_t, const String&)
{
    notImplemented();
}

void TextChecker::updateSpellingUIWithGrammarString(int64_t, const String&, const GrammarDetail&)
{
    notImplemented();
}

void TextChecker::getGuessesForWord(int64_t, const String&, const String&, int32_t, Vector<String>&, bool)
{
    notImplemented();
}

void TextChecker::learnWord(int64_t, const String&)
{
    notImplemented();
}

void TextChecker::ignoreWord(int64_t, const String&)
{
    notImplemented();
}

void TextChecker::requestCheckingOfString(Ref<TextCheckerCompletion>&&, int32_t)
{
    notImplemented();
}

} // namespace WebKit
