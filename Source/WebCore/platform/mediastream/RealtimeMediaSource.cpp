/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)
#include "RealtimeMediaSource.h"

#include "Logging.h"
#include "MediaConstraints.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceCapabilities.h"
#include "RealtimeMediaSourceCenter.h"
#include <wtf/MainThread.h>
#include <wtf/UUID.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

RealtimeMediaSource::RealtimeMediaSource(Type type, String&& name, String&& deviceID, String&& hashSalt)
    : m_idHashSalt(WTFMove(hashSalt))
    , m_persistentID(WTFMove(deviceID))
    , m_type(type)
    , m_name(WTFMove(name))
{
    if (m_persistentID.isEmpty())
        m_persistentID = createCanonicalUUIDString();
    else
        m_hashedID = RealtimeMediaSourceCenter::singleton().hashStringWithSalt(m_persistentID, m_idHashSalt);
}

void RealtimeMediaSource::addObserver(RealtimeMediaSource::Observer& observer)
{
    auto locker = holdLock(m_observersLock);
    m_observers.add(&observer);
}

void RealtimeMediaSource::removeObserver(RealtimeMediaSource::Observer& observer)
{
    auto locker = holdLock(m_observersLock);

    m_observers.remove(&observer);
    if (m_observers.isEmpty())
        stop();
}

void RealtimeMediaSource::setInterrupted(bool interrupted, bool pageMuted)
{
    if (interrupted == m_interrupted)
        return;

    m_interrupted = interrupted;
    if (!interrupted && pageMuted)
        return;

    setMuted(interrupted);
}

void RealtimeMediaSource::setMuted(bool muted)
{
    if (muted)
        stop();
    else {
        if (interrupted())
            return;

        start();
    }

    notifyMutedChange(muted);
}

void RealtimeMediaSource::notifyMutedChange(bool muted)
{
    if (m_muted == muted)
        return;

    m_muted = muted;

    notifyMutedObservers();
}

void RealtimeMediaSource::forEachObserver(const WTF::Function<void(Observer&)>& apply) const
{
    Vector<Observer*> observersCopy;
    {
        auto locker = holdLock(m_observersLock);
        observersCopy = copyToVector(m_observers);
    }
    for (auto* observer : observersCopy) {
        auto locker = holdLock(m_observersLock);
        // Make sure the observer has not been destroyed.
        if (!m_observers.contains(observer))
            continue;
        apply(*observer);
    }
}

void RealtimeMediaSource::notifyMutedObservers() const
{
    forEachObserver([](auto& observer) {
        observer.sourceMutedChanged();
    });
}

void RealtimeMediaSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>)
{
}

void RealtimeMediaSource::notifySettingsDidChangeObservers(OptionSet<RealtimeMediaSourceSettings::Flag> flags)
{
    ASSERT(isMainThread());

    settingsDidChange(flags);

    if (m_pendingSettingsDidChangeNotification)
        return;
    m_pendingSettingsDidChangeNotification = true;

    scheduleDeferredTask([this] {
        m_pendingSettingsDidChangeNotification = false;
        forEachObserver([](auto& observer) {
            observer.sourceSettingsChanged();
        });
    });
}

void RealtimeMediaSource::videoSampleAvailable(MediaSample& mediaSample)
{
    forEachObserver([&](auto& observer) {
        observer.videoSampleAvailable(mediaSample);
    });
}

void RealtimeMediaSource::audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t numberOfFrames)
{
    forEachObserver([&](auto& observer) {
        observer.audioSamplesAvailable(time, audioData, description, numberOfFrames);
    });
}

void RealtimeMediaSource::start()
{
    if (m_isProducingData)
        return;

    m_isProducingData = true;
    startProducingData();

    if (!m_isProducingData)
        return;

    forEachObserver([](auto& observer) {
        observer.sourceStarted();
    });
}

void RealtimeMediaSource::stop()
{
    if (!m_isProducingData)
        return;

    m_isProducingData = false;
    stopProducingData();
}

void RealtimeMediaSource::requestStop(Observer* callingObserver)
{
    if (!m_isProducingData)
        return;

    bool hasObserverPreventingStopping = false;
    forEachObserver([&](auto& observer) {
        if (observer.preventSourceFromStopping())
            hasObserverPreventingStopping = true;
    });
    if (hasObserverPreventingStopping)
        return;

    stop();

    forEachObserver([callingObserver](auto& observer) {
        if (&observer != callingObserver)
            observer.sourceStopped();
    });
}

void RealtimeMediaSource::captureFailed()
{
    RELEASE_LOG_ERROR(MediaStream, "RealtimeMediaSource::captureFailed");

    m_isProducingData = false;
    m_captureDidFailed = true;

    forEachObserver([](auto& observer) {
        observer.sourceStopped();
    });
}

bool RealtimeMediaSource::supportsSizeAndFrameRate(std::optional<int>, std::optional<int>, std::optional<double>)
{
    // The size and frame rate are within the capability limits, so they are supported.
    return true;
}

bool RealtimeMediaSource::supportsSizeAndFrameRate(std::optional<IntConstraint> widthConstraint, std::optional<IntConstraint> heightConstraint, std::optional<DoubleConstraint> frameRateConstraint, String& badConstraint, double& distance)
{
    if (!widthConstraint && !heightConstraint && !frameRateConstraint)
        return true;

    auto& capabilities = this->capabilities();

    distance = std::numeric_limits<double>::infinity();

    std::optional<int> width;
    if (widthConstraint && capabilities.supportsWidth()) {
        double constraintDistance = fitnessDistance(*widthConstraint);
        if (std::isinf(constraintDistance)) {
            badConstraint = widthConstraint->name();
            return false;
        }

        distance = std::min(distance, constraintDistance);
        if (widthConstraint->isMandatory()) {
            auto range = capabilities.width();
            width = widthConstraint->valueForCapabilityRange(size().width(), range.rangeMin().asInt, range.rangeMax().asInt);
        }
    }

    std::optional<int> height;
    if (heightConstraint && capabilities.supportsHeight()) {
        double constraintDistance = fitnessDistance(*heightConstraint);
        if (std::isinf(constraintDistance)) {
            badConstraint = heightConstraint->name();
            return false;
        }

        distance = std::min(distance, constraintDistance);
        if (heightConstraint->isMandatory()) {
            auto range = capabilities.height();
            height = heightConstraint->valueForCapabilityRange(size().height(), range.rangeMin().asInt, range.rangeMax().asInt);
        }
    }

    std::optional<double> frameRate;
    if (frameRateConstraint && capabilities.supportsFrameRate()) {
        double constraintDistance = fitnessDistance(*frameRateConstraint);
        if (std::isinf(constraintDistance)) {
            badConstraint = frameRateConstraint->name();
            return false;
        }

        distance = std::min(distance, constraintDistance);
        if (frameRateConstraint->isMandatory()) {
            auto range = capabilities.frameRate();
            frameRate = frameRateConstraint->valueForCapabilityRange(this->frameRate(), range.rangeMin().asDouble, range.rangeMax().asDouble);
        }
    }

    // Each of the non-null values is supported individually, see if they all can be applied at the same time.
    if (!supportsSizeAndFrameRate(WTFMove(width), WTFMove(height), WTFMove(frameRate))) {
        if (widthConstraint)
            badConstraint = widthConstraint->name();
        else if (heightConstraint)
            badConstraint = heightConstraint->name();
        else
            badConstraint = frameRateConstraint->name();
        return false;
    }

    return true;
}

double RealtimeMediaSource::fitnessDistance(const MediaConstraint& constraint)
{
    auto& capabilities = this->capabilities();

    switch (constraint.constraintType()) {
    case MediaConstraintType::Width: {
        ASSERT(constraint.isInt());
        if (!capabilities.supportsWidth())
            return 0;

        auto range = capabilities.width();
        return downcast<IntConstraint>(constraint).fitnessDistance(range.rangeMin().asInt, range.rangeMax().asInt);
        break;
    }

    case MediaConstraintType::Height: {
        ASSERT(constraint.isInt());
        if (!capabilities.supportsHeight())
            return 0;

        auto range = capabilities.height();
        return downcast<IntConstraint>(constraint).fitnessDistance(range.rangeMin().asInt, range.rangeMax().asInt);
        break;
    }

    case MediaConstraintType::FrameRate: {
        ASSERT(constraint.isDouble());
        if (!capabilities.supportsFrameRate())
            return 0;

        auto range = capabilities.frameRate();
        return downcast<DoubleConstraint>(constraint).fitnessDistance(range.rangeMin().asDouble, range.rangeMax().asDouble);
        break;
    }

    case MediaConstraintType::AspectRatio: {
        ASSERT(constraint.isDouble());
        if (!capabilities.supportsAspectRatio())
            return 0;

        auto range = capabilities.aspectRatio();
        return downcast<DoubleConstraint>(constraint).fitnessDistance(range.rangeMin().asDouble, range.rangeMax().asDouble);
        break;
    }

    case MediaConstraintType::Volume: {
        ASSERT(constraint.isDouble());
        if (!capabilities.supportsVolume())
            return 0;

        auto range = capabilities.volume();
        return downcast<DoubleConstraint>(constraint).fitnessDistance(range.rangeMin().asDouble, range.rangeMax().asDouble);
        break;
    }

    case MediaConstraintType::SampleRate: {
        ASSERT(constraint.isInt());
        if (!capabilities.supportsSampleRate())
            return 0;

        if (auto discreteRates = discreteSampleRates())
            return downcast<IntConstraint>(constraint).fitnessDistance(*discreteRates);

        auto range = capabilities.sampleRate();
        return downcast<IntConstraint>(constraint).fitnessDistance(range.rangeMin().asInt, range.rangeMax().asInt);
        break;
    }

    case MediaConstraintType::SampleSize: {
        ASSERT(constraint.isInt());
        if (!capabilities.supportsSampleSize())
            return 0;

        if (auto discreteSizes = discreteSampleSizes())
            return downcast<IntConstraint>(constraint).fitnessDistance(*discreteSizes);

        auto range = capabilities.sampleSize();
        return downcast<IntConstraint>(constraint).fitnessDistance(range.rangeMin().asInt, range.rangeMax().asInt);
        break;
    }

    case MediaConstraintType::FacingMode: {
        ASSERT(constraint.isString());
        if (!capabilities.supportsFacingMode())
            return 0;

        auto& modes = capabilities.facingMode();
        Vector<String> supportedModes;
        supportedModes.reserveInitialCapacity(modes.size());
        for (auto& mode : modes)
            supportedModes.uncheckedAppend(RealtimeMediaSourceSettings::facingMode(mode));
        return downcast<StringConstraint>(constraint).fitnessDistance(supportedModes);
        break;
    }

    case MediaConstraintType::EchoCancellation: {
        ASSERT(constraint.isBoolean());
        if (!capabilities.supportsEchoCancellation())
            return 0;

        bool echoCancellationReadWrite = capabilities.echoCancellation() == RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite;
        return downcast<BooleanConstraint>(constraint).fitnessDistance(echoCancellationReadWrite);
        break;
    }

    case MediaConstraintType::DeviceId:
        ASSERT_NOT_REACHED();
        break;

    case MediaConstraintType::GroupId: {
        ASSERT(constraint.isString());
        if (!capabilities.supportsDeviceId())
            return 0;

        return downcast<StringConstraint>(constraint).fitnessDistance(settings().groupId());
        break;
    }

    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
        break;

    case MediaConstraintType::Unknown:
        // Unknown (or unsupported) constraints should be ignored.
        break;
    }

    return 0;
}

template <typename ValueType>
static void applyNumericConstraint(const NumericConstraint<ValueType>& constraint, ValueType current, std::optional<Vector<ValueType>> discreteCapabilityValues, ValueType capabilityMin, ValueType capabilityMax, RealtimeMediaSource& source, void (RealtimeMediaSource::*applier)(ValueType))
{
    if (discreteCapabilityValues) {
        int value = constraint.valueForDiscreteCapabilityValues(current, *discreteCapabilityValues);
        if (value != current)
            (source.*applier)(value);
        return;
    }

    ValueType value = constraint.valueForCapabilityRange(current, capabilityMin, capabilityMax);
    if (value != current)
        (source.*applier)(value);
}

void RealtimeMediaSource::setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    IntSize size = this->size();
    if (width)
        size.setWidth(width.value());
    if (height)
        size.setHeight(height.value());
    setSize(size);
    if (frameRate)
        setFrameRate(frameRate.value());
}

void RealtimeMediaSource::applyConstraint(const MediaConstraint& constraint)
{
    auto& capabilities = this->capabilities();
    switch (constraint.constraintType()) {
    case MediaConstraintType::Width:
        ASSERT_NOT_REACHED();
        break;

    case MediaConstraintType::Height:
        ASSERT_NOT_REACHED();
        break;

    case MediaConstraintType::FrameRate:
        ASSERT_NOT_REACHED();
        break;

    case MediaConstraintType::AspectRatio: {
        ASSERT(constraint.isDouble());
        if (!capabilities.supportsAspectRatio())
            return;

        auto range = capabilities.aspectRatio();
        applyNumericConstraint(downcast<DoubleConstraint>(constraint), aspectRatio(), { }, range.rangeMin().asDouble, range.rangeMax().asDouble, *this, &RealtimeMediaSource::setAspectRatio);
        break;
    }

    case MediaConstraintType::Volume: {
        ASSERT(constraint.isDouble());
        if (!capabilities.supportsVolume())
            return;

        auto range = capabilities.volume();
        applyNumericConstraint(downcast<DoubleConstraint>(constraint), volume(), { }, range.rangeMin().asDouble, range.rangeMax().asDouble, *this, &RealtimeMediaSource::setVolume);
        break;
    }

    case MediaConstraintType::SampleRate: {
        ASSERT(constraint.isInt());
        if (!capabilities.supportsSampleRate())
            return;

        auto range = capabilities.sampleRate();
        applyNumericConstraint(downcast<IntConstraint>(constraint), sampleRate(), discreteSampleRates(), range.rangeMin().asInt, range.rangeMax().asInt, *this, &RealtimeMediaSource::setSampleRate);
        break;
    }

    case MediaConstraintType::SampleSize: {
        ASSERT(constraint.isInt());
        if (!capabilities.supportsSampleSize())
            return;

        auto range = capabilities.sampleSize();
        applyNumericConstraint(downcast<IntConstraint>(constraint), sampleSize(), { }, range.rangeMin().asInt, range.rangeMax().asInt, *this, &RealtimeMediaSource::setSampleSize);
        break;
    }

    case MediaConstraintType::EchoCancellation: {
        ASSERT(constraint.isBoolean());
        if (!capabilities.supportsEchoCancellation())
            return;

        bool setting;
        const BooleanConstraint& boolConstraint = downcast<BooleanConstraint>(constraint);
        if (boolConstraint.getExact(setting) || boolConstraint.getIdeal(setting))
            setEchoCancellation(setting);
        break;
    }

    case MediaConstraintType::FacingMode: {
        ASSERT(constraint.isString());
        if (!capabilities.supportsFacingMode())
            return;

        auto& supportedModes = capabilities.facingMode();
        auto filter = [supportedModes](const String& modeString) {
            auto mode = RealtimeMediaSourceSettings::videoFacingModeEnum(modeString);
            for (auto& supportedMode : supportedModes) {
                if (mode == supportedMode)
                    return true;
            }
            return false;
        };

        auto modeString = downcast<StringConstraint>(constraint).find(WTFMove(filter));
        if (!modeString.isEmpty())
            setFacingMode(RealtimeMediaSourceSettings::videoFacingModeEnum(modeString));
        break;
    }

    case MediaConstraintType::DeviceId:
    case MediaConstraintType::GroupId:
        ASSERT(constraint.isString());
        // There is nothing to do here, neither can be changed.
        break;

    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
        ASSERT(constraint.isBoolean());
        break;

    case MediaConstraintType::Unknown:
        break;
    }
}

bool RealtimeMediaSource::selectSettings(const MediaConstraints& constraints, FlattenedConstraint& candidates, String& failedConstraint, SelectType type)
{
    m_fitnessScore = std::numeric_limits<double>::infinity();

    // https://w3c.github.io/mediacapture-main/#dfn-selectsettings
    //
    // 1. Each constraint specifies one or more values (or a range of values) for its property.
    //    A property may appear more than once in the list of 'advanced' ConstraintSets. If an
    //    empty object or list has been given as the value for a constraint, it must be interpreted
    //    as if the constraint were not specified (in other words, an empty constraint == no constraint).
    //
    //    Note that unknown properties are discarded by WebIDL, which means that unknown/unsupported required
    //    constraints will silently disappear. To avoid this being a surprise, application authors are
    //    expected to first use the getSupportedConstraints() method as shown in the Examples below.

    // 2. Let object be the ConstrainablePattern object on which this algorithm is applied. Let copy be an
    //    unconstrained copy of object (i.e., copy should behave as if it were object with all ConstraintSets
    //    removed.)

    // 3. For every possible settings dictionary of copy compute its fitness distance, treating bare values of
    //    properties as ideal values. Let candidates be the set of settings dictionaries for which the fitness
    //    distance is finite.

    failedConstraint = emptyString();

    // Check width, height and frame rate jointly, because while they may be supported individually the combination may not be supported.
    double distance = std::numeric_limits<double>::infinity();
    if (!supportsSizeAndFrameRate(constraints.mandatoryConstraints.width(), constraints.mandatoryConstraints.height(), constraints.mandatoryConstraints.frameRate(), failedConstraint, m_fitnessScore))
        return false;

    constraints.mandatoryConstraints.filter([&](const MediaConstraint& constraint) {
        if (!supportsConstraint(constraint))
            return false;

        if (constraint.constraintType() == MediaConstraintType::Width || constraint.constraintType() == MediaConstraintType::Height || constraint.constraintType() == MediaConstraintType::FrameRate) {
            candidates.set(constraint);
            return false;
        }

        // The deviceId can't be changed, and the constraint value is the hashed device ID, so verify that the
        // device's unique ID hashes to the constraint value but don't include the constraint in the flattened
        // constraint set.
        if (constraint.constraintType() == MediaConstraintType::DeviceId) {
            if (type == SelectType::ForApplyConstraints)
                return false;

            ASSERT(constraint.isString());
            ASSERT(!m_hashedID.isEmpty());

            double constraintDistance = downcast<StringConstraint>(constraint).fitnessDistance(m_hashedID);
            if (std::isinf(constraintDistance)) {
                failedConstraint = constraint.name();
                return true;
            }

            return false;
        }

        double constraintDistance = fitnessDistance(constraint);
        if (std::isinf(constraintDistance)) {
            failedConstraint = constraint.name();
            return true;
        }

        distance = std::min(distance, constraintDistance);
        candidates.set(constraint);
        return false;
    });

    if (!failedConstraint.isEmpty())
        return false;

    m_fitnessScore = distance;

    // 4. If candidates is empty, return undefined as the result of the SelectSettings() algorithm.
    if (candidates.isEmpty())
        return true;

    // 5. Iterate over the 'advanced' ConstraintSets in newConstraints in the order in which they were specified.
    //    For each ConstraintSet:

    // 5.1 compute the fitness distance between it and each settings dictionary in candidates, treating bare
    //     values of properties as exact.
    Vector<std::pair<double, MediaTrackConstraintSetMap>> supportedConstraints;

    for (const auto& advancedConstraint : constraints.advancedConstraints) {
        double constraintDistance = 0;
        bool supported = false;

        if (advancedConstraint.width() || advancedConstraint.height() || advancedConstraint.frameRate()) {
            String dummy;
            if (!supportsSizeAndFrameRate(advancedConstraint.width(), advancedConstraint.height(), advancedConstraint.frameRate(), dummy, constraintDistance))
                continue;

            supported = true;
        }

        advancedConstraint.forEach([&](const MediaConstraint& constraint) {

            if (constraint.constraintType() == MediaConstraintType::Width || constraint.constraintType() == MediaConstraintType::Height || constraint.constraintType() == MediaConstraintType::FrameRate)
                return;

            distance = fitnessDistance(constraint);
            constraintDistance += distance;
            if (!std::isinf(distance))
                supported = true;
        });

        m_fitnessScore = std::min(m_fitnessScore, constraintDistance);

        // 5.2 If the fitness distance is finite for one or more settings dictionaries in candidates, keep those
        //     settings dictionaries in candidates, discarding others.
        //     If the fitness distance is infinite for all settings dictionaries in candidates, ignore this ConstraintSet.
        if (supported)
            supportedConstraints.append({constraintDistance, advancedConstraint});
    }

    // 6. Select one settings dictionary from candidates, and return it as the result of the SelectSettings() algorithm.
    //    The UA should use the one with the smallest fitness distance, as calculated in step 3.
    if (!supportedConstraints.isEmpty()) {
        supportedConstraints.removeAllMatching([&](const std::pair<double, MediaTrackConstraintSetMap>& pair) -> bool {
            return std::isinf(pair.first) || pair.first > m_fitnessScore;
        });

        if (!supportedConstraints.isEmpty()) {
            auto& advancedConstraint = supportedConstraints[0].second;
            advancedConstraint.forEach([&](const MediaConstraint& constraint) {
                candidates.merge(constraint);
            });

            m_fitnessScore = std::min(m_fitnessScore, supportedConstraints[0].first);
        }
    }

    return true;
}

bool RealtimeMediaSource::supportsConstraint(const MediaConstraint& constraint)
{
    auto& capabilities = this->capabilities();

    switch (constraint.constraintType()) {
    case MediaConstraintType::Width:
        ASSERT(constraint.isInt());
        return capabilities.supportsWidth();
        break;

    case MediaConstraintType::Height:
        ASSERT(constraint.isInt());
        return capabilities.supportsHeight();
        break;

    case MediaConstraintType::FrameRate:
        ASSERT(constraint.isDouble());
        return capabilities.supportsFrameRate();
        break;

    case MediaConstraintType::AspectRatio:
        ASSERT(constraint.isDouble());
        return capabilities.supportsAspectRatio();
        break;

    case MediaConstraintType::Volume:
        ASSERT(constraint.isDouble());
        return capabilities.supportsVolume();
        break;

    case MediaConstraintType::SampleRate:
        ASSERT(constraint.isInt());
        return capabilities.supportsSampleRate();
        break;

    case MediaConstraintType::SampleSize:
        ASSERT(constraint.isInt());
        return capabilities.supportsSampleSize();
        break;

    case MediaConstraintType::FacingMode:
        ASSERT(constraint.isString());
        return capabilities.supportsFacingMode();
        break;

    case MediaConstraintType::EchoCancellation:
        ASSERT(constraint.isBoolean());
        return capabilities.supportsEchoCancellation();
        break;

    case MediaConstraintType::DeviceId:
        ASSERT(constraint.isString());
        return capabilities.supportsDeviceId();
        break;

    case MediaConstraintType::GroupId:
        ASSERT(constraint.isString());
        return capabilities.supportsDeviceId();
        break;

    case MediaConstraintType::DisplaySurface:
    case MediaConstraintType::LogicalSurface:
        // https://www.w3.org/TR/screen-capture/#new-constraints-for-captured-display-surfaces
        // 5.2.1 New Constraints for Captured Display Surfaces
        // Since the source of media cannot be changed after a MediaStreamTrack has been returned,
        // these constraints cannot be changed by an application.
        return false;
        break;

    case MediaConstraintType::Unknown:
        // Unknown (or unsupported) constraints should be ignored.
        break;
    }
    
    return false;
}

bool RealtimeMediaSource::supportsConstraints(const MediaConstraints& constraints, String& invalidConstraint)
{
    ASSERT(constraints.isValid);

    FlattenedConstraint candidates;
    if (!selectSettings(constraints, candidates, invalidConstraint, SelectType::ForSupportsConstraints))
        return false;
    
    return true;
}

void RealtimeMediaSource::applyConstraints(const FlattenedConstraint& constraints)
{
    if (constraints.isEmpty())
        return;

    beginConfiguration();

    auto& capabilities = this->capabilities();

    std::optional<int> width;
    if (const MediaConstraint* constraint = constraints.find(MediaConstraintType::Width)) {
        ASSERT(constraint->isInt());
        if (capabilities.supportsWidth()) {
            auto range = capabilities.width();
            width = downcast<IntConstraint>(*constraint).valueForCapabilityRange(size().width(), range.rangeMin().asInt, range.rangeMax().asInt);
        }
    }

    std::optional<int> height;
    if (const MediaConstraint* constraint = constraints.find(MediaConstraintType::Height)) {
        ASSERT(constraint->isInt());
        if (capabilities.supportsHeight()) {
            auto range = capabilities.height();
            height = downcast<IntConstraint>(*constraint).valueForCapabilityRange(size().height(), range.rangeMin().asInt, range.rangeMax().asInt);
        }
    }

    std::optional<double> frameRate;
    if (const MediaConstraint* constraint = constraints.find(MediaConstraintType::FrameRate)) {
        ASSERT(constraint->isDouble());
        if (capabilities.supportsFrameRate()) {
            auto range = capabilities.frameRate();
            frameRate = downcast<DoubleConstraint>(*constraint).valueForCapabilityRange(this->frameRate(), range.rangeMin().asDouble, range.rangeMax().asDouble);
        }
    }

    if (width || height || frameRate)
        setSizeAndFrameRate(WTFMove(width), WTFMove(height), WTFMove(frameRate));

    for (auto& variant : constraints) {
        if (variant.constraintType() == MediaConstraintType::Width || variant.constraintType() == MediaConstraintType::Height || variant.constraintType() == MediaConstraintType::FrameRate)
            continue;

        applyConstraint(variant);
    }

    commitConfiguration();
}

std::optional<std::pair<String, String>> RealtimeMediaSource::applyConstraints(const MediaConstraints& constraints)
{
    ASSERT(constraints.isValid);

    FlattenedConstraint candidates;
    String failedConstraint;
    if (!selectSettings(constraints, candidates, failedConstraint, SelectType::ForApplyConstraints))
        return { { failedConstraint, "Constraint not supported"_s } };

    applyConstraints(candidates);
    return std::nullopt;
}

void RealtimeMediaSource::applyConstraints(const MediaConstraints& constraints, SuccessHandler&& successHandler, FailureHandler&& failureHandler)
{
    auto result = applyConstraints(constraints);
    if (!result && successHandler)
        successHandler();
    else if (result && failureHandler)
        failureHandler(result.value().first, result.value().second);
}

void RealtimeMediaSource::setSize(const IntSize& size)
{
    if (size == m_size)
        return;

    OptionSet<RealtimeMediaSourceSettings::Flag> changed;
    if (m_size.width() != size.width())
        changed.add(RealtimeMediaSourceSettings::Flag::Width);
    if (m_size.height() != size.height())
        changed.add(RealtimeMediaSourceSettings::Flag::Height);

    m_size = size;
    notifySettingsDidChangeObservers(changed);
}

void RealtimeMediaSource::setFrameRate(double rate)
{
    if (m_frameRate == rate)
        return;

    m_frameRate = rate;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::FrameRate);
}

void RealtimeMediaSource::setAspectRatio(double ratio)
{
    if (m_aspectRatio == ratio)
        return;

    m_aspectRatio = ratio;
    m_size.setHeight(m_size.width() / ratio);
    notifySettingsDidChangeObservers({ RealtimeMediaSourceSettings::Flag::AspectRatio, RealtimeMediaSourceSettings::Flag::Height });
}

void RealtimeMediaSource::setFacingMode(RealtimeMediaSourceSettings::VideoFacingMode mode)
{
    if (m_facingMode == mode)
        return;

    m_facingMode = mode;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::FacingMode);
}

void RealtimeMediaSource::setVolume(double volume)
{
    if (m_volume == volume)
        return;

    m_volume = volume;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::Volume);
}

void RealtimeMediaSource::setSampleRate(int rate)
{
    if (m_sampleRate == rate)
        return;

    m_sampleRate = rate;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::SampleRate);
}

std::optional<Vector<int>> RealtimeMediaSource::discreteSampleRates() const
{
    return std::nullopt;
}

void RealtimeMediaSource::setSampleSize(int size)
{
    if (m_sampleSize == size)
        return;

    m_sampleSize = size;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::SampleSize);
}

std::optional<Vector<int>> RealtimeMediaSource::discreteSampleSizes() const
{
    return std::nullopt;
}

void RealtimeMediaSource::setEchoCancellation(bool echoCancellation)
{
    if (m_echoCancellation == echoCancellation)
        return;

    m_echoCancellation = echoCancellation;
    notifySettingsDidChangeObservers(RealtimeMediaSourceSettings::Flag::EchoCancellation);
}

void RealtimeMediaSource::scheduleDeferredTask(WTF::Function<void()>&& function)
{
    ASSERT(function);
    callOnMainThread([weakThis = makeWeakPtr(*this), function = WTFMove(function)] {
        if (!weakThis)
            return;

        function();
    });
}

const String& RealtimeMediaSource::hashedId() const
{
    ASSERT(!m_hashedID.isEmpty());
    return m_hashedID;
}

String RealtimeMediaSource::deviceIDHashSalt() const
{
    return m_idHashSalt;
}

RealtimeMediaSource::Observer::~Observer()
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
