/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSTestNode.h"

#include "JSDOMAttribute.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructor.h"
#include "JSDOMConvertBoolean.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMIterator.h"
#include "JSDOMOperation.h"
#include "JSDOMOperationReturningPromise.h"
#include "JSDOMWrapperCache.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <wtf/GetPtr.h>
#include <wtf/PointerPreparations.h>


namespace WebCore {
using namespace JSC;

// Functions

JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionTestWorkerPromise(JSC::ExecState*);
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionCalculateSecretResult(JSC::ExecState*);
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionGetSecretBoolean(JSC::ExecState*);
#if ENABLE(TEST_FEATURE)
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionTestFeatureGetSecretBoolean(JSC::ExecState*);
#endif
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionSymbolIterator(JSC::ExecState*);
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionEntries(JSC::ExecState*);
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionKeys(JSC::ExecState*);
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionValues(JSC::ExecState*);
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionForEach(JSC::ExecState*);
JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionToJSON(JSC::ExecState*);

// Attributes

JSC::EncodedJSValue jsTestNodeConstructor(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);
bool setJSTestNodeConstructor(JSC::ExecState*, JSC::EncodedJSValue, JSC::EncodedJSValue);
JSC::EncodedJSValue jsTestNodeName(JSC::ExecState*, JSC::EncodedJSValue, JSC::PropertyName);
bool setJSTestNodeName(JSC::ExecState*, JSC::EncodedJSValue, JSC::EncodedJSValue);

class JSTestNodePrototype : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    static JSTestNodePrototype* create(JSC::VM& vm, JSDOMGlobalObject* globalObject, JSC::Structure* structure)
    {
        JSTestNodePrototype* ptr = new (NotNull, JSC::allocateCell<JSTestNodePrototype>(vm.heap)) JSTestNodePrototype(vm, globalObject, structure);
        ptr->finishCreation(vm);
        return ptr;
    }

    DECLARE_INFO;
    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

private:
    JSTestNodePrototype(JSC::VM& vm, JSC::JSGlobalObject*, JSC::Structure* structure)
        : JSC::JSNonFinalObject(vm, structure)
    {
    }

    void finishCreation(JSC::VM&);
};

using JSTestNodeConstructor = JSDOMConstructor<JSTestNode>;

template<> EncodedJSValue JSC_HOST_CALL JSTestNodeConstructor::construct(ExecState* state)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);
    auto* castedThis = jsCast<JSTestNodeConstructor*>(state->jsCallee());
    ASSERT(castedThis);
    auto object = TestNode::create();
    return JSValue::encode(toJSNewlyCreated<IDLInterface<TestNode>>(*state, *castedThis->globalObject(), WTFMove(object)));
}

template<> JSValue JSTestNodeConstructor::prototypeForStructure(JSC::VM& vm, const JSDOMGlobalObject& globalObject)
{
    return JSNode::getConstructor(vm, &globalObject);
}

template<> void JSTestNodeConstructor::initializeProperties(VM& vm, JSDOMGlobalObject& globalObject)
{
    putDirect(vm, vm.propertyNames->prototype, JSTestNode::prototype(vm, globalObject), JSC::PropertyAttribute::DontDelete | JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->name, jsNontrivialString(&vm, String("TestNode"_s)), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
    putDirect(vm, vm.propertyNames->length, jsNumber(0), JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum);
}

template<> const ClassInfo JSTestNodeConstructor::s_info = { "TestNode", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestNodeConstructor) };

/* Hash table for prototype */

static const HashTableValue JSTestNodePrototypeTableValues[] =
{
    { "constructor", static_cast<unsigned>(JSC::PropertyAttribute::DontEnum), NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestNodeConstructor), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(setJSTestNodeConstructor) } },
    { "name", static_cast<unsigned>(JSC::PropertyAttribute::CustomAccessor | JSC::PropertyAttribute::DOMAttribute), NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(jsTestNodeName), (intptr_t) static_cast<PutPropertySlot::PutValueFunc>(setJSTestNodeName) } },
    { "testWorkerPromise", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionTestWorkerPromise), (intptr_t) (0) } },
    { "calculateSecretResult", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionCalculateSecretResult), (intptr_t) (0) } },
    { "getSecretBoolean", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionGetSecretBoolean), (intptr_t) (0) } },
#if ENABLE(TEST_FEATURE)
    { "testFeatureGetSecretBoolean", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionTestFeatureGetSecretBoolean), (intptr_t) (0) } },
#else
    { 0, 0, NoIntrinsic, { 0, 0 } },
#endif
    { "entries", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionEntries), (intptr_t) (0) } },
    { "keys", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionKeys), (intptr_t) (0) } },
    { "values", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionValues), (intptr_t) (0) } },
    { "forEach", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionForEach), (intptr_t) (1) } },
    { "toJSON", static_cast<unsigned>(JSC::PropertyAttribute::Function), NoIntrinsic, { (intptr_t)static_cast<RawNativeFunction>(jsTestNodePrototypeFunctionToJSON), (intptr_t) (0) } },
};

const ClassInfo JSTestNodePrototype::s_info = { "TestNodePrototype", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestNodePrototype) };

void JSTestNodePrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    reifyStaticProperties(vm, JSTestNode::info(), JSTestNodePrototypeTableValues, *this);
    if (!jsCast<JSDOMGlobalObject*>(globalObject())->scriptExecutionContext()->isSecureContext()) {
        auto propertyName = Identifier::fromString(&vm, reinterpret_cast<const LChar*>("calculateSecretResult"), strlen("calculateSecretResult"));
        VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
        JSObject::deleteProperty(this, globalObject()->globalExec(), propertyName);
    }
    if (!jsCast<JSDOMGlobalObject*>(globalObject())->scriptExecutionContext()->isSecureContext()) {
        auto propertyName = Identifier::fromString(&vm, reinterpret_cast<const LChar*>("getSecretBoolean"), strlen("getSecretBoolean"));
        VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
        JSObject::deleteProperty(this, globalObject()->globalExec(), propertyName);
    }
#if ENABLE(TEST_FEATURE)
    if (!(jsCast<JSDOMGlobalObject*>(globalObject())->scriptExecutionContext()->isSecureContext() && RuntimeEnabledFeatures::sharedFeatures().testFeatureEnabled())) {
        auto propertyName = Identifier::fromString(&vm, reinterpret_cast<const LChar*>("testFeatureGetSecretBoolean"), strlen("testFeatureGetSecretBoolean"));
        VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
        JSObject::deleteProperty(this, globalObject()->globalExec(), propertyName);
    }
#endif
    if (!RuntimeEnabledFeatures::sharedFeatures().domIteratorEnabled()) {
        auto propertyName = Identifier::fromString(&vm, reinterpret_cast<const LChar*>("entries"), strlen("entries"));
        VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
        JSObject::deleteProperty(this, globalObject()->globalExec(), propertyName);
    }
    if (!RuntimeEnabledFeatures::sharedFeatures().domIteratorEnabled()) {
        auto propertyName = Identifier::fromString(&vm, reinterpret_cast<const LChar*>("keys"), strlen("keys"));
        VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
        JSObject::deleteProperty(this, globalObject()->globalExec(), propertyName);
    }
    if (!RuntimeEnabledFeatures::sharedFeatures().domIteratorEnabled()) {
        auto propertyName = Identifier::fromString(&vm, reinterpret_cast<const LChar*>("values"), strlen("values"));
        VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
        JSObject::deleteProperty(this, globalObject()->globalExec(), propertyName);
    }
    if (!RuntimeEnabledFeatures::sharedFeatures().domIteratorEnabled()) {
        auto propertyName = Identifier::fromString(&vm, reinterpret_cast<const LChar*>("forEach"), strlen("forEach"));
        VM::DeletePropertyModeScope scope(vm, VM::DeletePropertyMode::IgnoreConfigurable);
        JSObject::deleteProperty(this, globalObject()->globalExec(), propertyName);
    }
    putDirect(vm, vm.propertyNames->iteratorSymbol, getDirect(vm, vm.propertyNames->builtinNames().entriesPublicName()), static_cast<unsigned>(JSC::PropertyAttribute::DontEnum));
}

const ClassInfo JSTestNode::s_info = { "TestNode", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestNode) };

JSTestNode::JSTestNode(Structure* structure, JSDOMGlobalObject& globalObject, Ref<TestNode>&& impl)
    : JSNode(structure, globalObject, WTFMove(impl))
{
}

void JSTestNode::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

}

JSObject* JSTestNode::createPrototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return JSTestNodePrototype::create(vm, &globalObject, JSTestNodePrototype::createStructure(vm, &globalObject, JSNode::prototype(vm, globalObject)));
}

JSObject* JSTestNode::prototype(VM& vm, JSDOMGlobalObject& globalObject)
{
    return getDOMPrototype<JSTestNode>(vm, globalObject);
}

JSValue JSTestNode::getConstructor(VM& vm, const JSGlobalObject* globalObject)
{
    return getDOMConstructor<JSTestNodeConstructor>(vm, *jsCast<const JSDOMGlobalObject*>(globalObject));
}

template<> inline JSTestNode* IDLAttribute<JSTestNode>::cast(ExecState& state, EncodedJSValue thisValue)
{
    return jsDynamicCast<JSTestNode*>(state.vm(), JSValue::decode(thisValue));
}

template<> inline JSTestNode* IDLOperation<JSTestNode>::cast(ExecState& state)
{
    return jsDynamicCast<JSTestNode*>(state.vm(), state.thisValue());
}

EncodedJSValue jsTestNodeConstructor(ExecState* state, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicCast<JSTestNodePrototype*>(vm, JSValue::decode(thisValue));
    if (UNLIKELY(!prototype))
        return throwVMTypeError(state, throwScope);
    return JSValue::encode(JSTestNode::getConstructor(state->vm(), prototype->globalObject()));
}

bool setJSTestNodeConstructor(ExecState* state, EncodedJSValue thisValue, EncodedJSValue encodedValue)
{
    VM& vm = state->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* prototype = jsDynamicCast<JSTestNodePrototype*>(vm, JSValue::decode(thisValue));
    if (UNLIKELY(!prototype)) {
        throwVMTypeError(state, throwScope);
        return false;
    }
    // Shadowing a built-in constructor
    return prototype->putDirect(vm, vm.propertyNames->constructor, JSValue::decode(encodedValue));
}

static inline JSValue jsTestNodeNameGetter(ExecState& state, JSTestNode& thisObject, ThrowScope& throwScope)
{
    UNUSED_PARAM(throwScope);
    UNUSED_PARAM(state);
    auto& impl = thisObject.wrapped();
    JSValue result = toJS<IDLDOMString>(state, throwScope, impl.name());
    return result;
}

EncodedJSValue jsTestNodeName(ExecState* state, EncodedJSValue thisValue, PropertyName)
{
    return IDLAttribute<JSTestNode>::get<jsTestNodeNameGetter, CastedThisErrorBehavior::Assert>(*state, thisValue, "name");
}

static inline bool setJSTestNodeNameSetter(ExecState& state, JSTestNode& thisObject, JSValue value, ThrowScope& throwScope)
{
    UNUSED_PARAM(throwScope);
    auto& impl = thisObject.wrapped();
    auto nativeValue = convert<IDLDOMString>(state, value);
    RETURN_IF_EXCEPTION(throwScope, false);
    AttributeSetter::call(state, throwScope, [&] {
        return impl.setName(WTFMove(nativeValue));
    });
    return true;
}

bool setJSTestNodeName(ExecState* state, EncodedJSValue thisValue, EncodedJSValue encodedValue)
{
    return IDLAttribute<JSTestNode>::set<setJSTestNodeNameSetter>(*state, thisValue, encodedValue, "name");
}

static inline JSC::EncodedJSValue jsTestNodePrototypeFunctionTestWorkerPromiseBody(JSC::ExecState* state, typename IDLOperationReturningPromise<JSTestNode>::ClassParameter castedThis, Ref<DeferredPromise>&& promise, JSC::ThrowScope& throwScope)
{
    UNUSED_PARAM(state);
    UNUSED_PARAM(throwScope);
    auto& impl = castedThis->wrapped();
    impl.testWorkerPromise(WTFMove(promise));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionTestWorkerPromise(ExecState* state)
{
    return IDLOperationReturningPromise<JSTestNode>::call<jsTestNodePrototypeFunctionTestWorkerPromiseBody, PromiseExecutionScope::WindowOrWorker>(*state, "testWorkerPromise");
}

static inline JSC::EncodedJSValue jsTestNodePrototypeFunctionCalculateSecretResultBody(JSC::ExecState* state, typename IDLOperationReturningPromise<JSTestNode>::ClassParameter castedThis, Ref<DeferredPromise>&& promise, JSC::ThrowScope& throwScope)
{
    UNUSED_PARAM(state);
    UNUSED_PARAM(throwScope);
    auto& impl = castedThis->wrapped();
    impl.calculateSecretResult(WTFMove(promise));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionCalculateSecretResult(ExecState* state)
{
    return IDLOperationReturningPromise<JSTestNode>::call<jsTestNodePrototypeFunctionCalculateSecretResultBody, PromiseExecutionScope::WindowOrWorker>(*state, "calculateSecretResult");
}

static inline JSC::EncodedJSValue jsTestNodePrototypeFunctionGetSecretBooleanBody(JSC::ExecState* state, typename IDLOperation<JSTestNode>::ClassParameter castedThis, JSC::ThrowScope& throwScope)
{
    UNUSED_PARAM(state);
    UNUSED_PARAM(throwScope);
    auto& impl = castedThis->wrapped();
    return JSValue::encode(toJS<IDLBoolean>(impl.getSecretBoolean()));
}

EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionGetSecretBoolean(ExecState* state)
{
    return IDLOperation<JSTestNode>::call<jsTestNodePrototypeFunctionGetSecretBooleanBody>(*state, "getSecretBoolean");
}

#if ENABLE(TEST_FEATURE)
static inline JSC::EncodedJSValue jsTestNodePrototypeFunctionTestFeatureGetSecretBooleanBody(JSC::ExecState* state, typename IDLOperation<JSTestNode>::ClassParameter castedThis, JSC::ThrowScope& throwScope)
{
    UNUSED_PARAM(state);
    UNUSED_PARAM(throwScope);
    auto& impl = castedThis->wrapped();
    return JSValue::encode(toJS<IDLBoolean>(impl.testFeatureGetSecretBoolean()));
}

EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionTestFeatureGetSecretBoolean(ExecState* state)
{
    return IDLOperation<JSTestNode>::call<jsTestNodePrototypeFunctionTestFeatureGetSecretBooleanBody>(*state, "testFeatureGetSecretBoolean");
}

#endif

struct TestNodeIteratorTraits {
    static constexpr JSDOMIteratorType type = JSDOMIteratorType::Set;
    using KeyType = void;
    using ValueType = IDLInterface<TestNode>;
};

using TestNodeIterator = JSDOMIterator<JSTestNode, TestNodeIteratorTraits>;
using TestNodeIteratorPrototype = JSDOMIteratorPrototype<JSTestNode, TestNodeIteratorTraits>;

template<>
const JSC::ClassInfo TestNodeIterator::s_info = { "TestNode Iterator", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TestNodeIterator) };

template<>
const JSC::ClassInfo TestNodeIteratorPrototype::s_info = { "TestNode Iterator", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TestNodeIteratorPrototype) };

static inline EncodedJSValue jsTestNodePrototypeFunctionEntriesCaller(ExecState*, JSTestNode* thisObject, JSC::ThrowScope&)
{
    return JSValue::encode(iteratorCreate<TestNodeIterator>(*thisObject, IterationKind::Value));
}

JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionEntries(JSC::ExecState* state)
{
    return IDLOperation<JSTestNode>::call<jsTestNodePrototypeFunctionEntriesCaller>(*state, "entries");
}

static inline EncodedJSValue jsTestNodePrototypeFunctionKeysCaller(ExecState*, JSTestNode* thisObject, JSC::ThrowScope&)
{
    return JSValue::encode(iteratorCreate<TestNodeIterator>(*thisObject, IterationKind::Key));
}

JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionKeys(JSC::ExecState* state)
{
    return IDLOperation<JSTestNode>::call<jsTestNodePrototypeFunctionKeysCaller>(*state, "keys");
}

static inline EncodedJSValue jsTestNodePrototypeFunctionValuesCaller(ExecState*, JSTestNode* thisObject, JSC::ThrowScope&)
{
    return JSValue::encode(iteratorCreate<TestNodeIterator>(*thisObject, IterationKind::Value));
}

JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionValues(JSC::ExecState* state)
{
    return IDLOperation<JSTestNode>::call<jsTestNodePrototypeFunctionValuesCaller>(*state, "values");
}

static inline EncodedJSValue jsTestNodePrototypeFunctionForEachCaller(ExecState* state, JSTestNode* thisObject, JSC::ThrowScope& throwScope)
{
    return JSValue::encode(iteratorForEach<TestNodeIterator>(*state, *thisObject, throwScope));
}

JSC::EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionForEach(JSC::ExecState* state)
{
    return IDLOperation<JSTestNode>::call<jsTestNodePrototypeFunctionForEachCaller>(*state, "forEach");
}

JSC::JSObject* JSTestNode::serialize(ExecState& state, JSTestNode& thisObject, JSDOMGlobalObject& globalObject, ThrowScope& throwScope)
{
    auto& vm = state.vm();
    auto* result = constructEmptyObject(&state, globalObject.objectPrototype());

    auto nameValue = jsTestNodeNameGetter(state, thisObject, throwScope);
    throwScope.assertNoException();
    result->putDirect(vm, Identifier::fromString(&vm, "name"), nameValue);

    return result;
}

static inline EncodedJSValue jsTestNodePrototypeFunctionToJSONBody(ExecState* state, JSTestNode* thisObject, JSC::ThrowScope& throwScope)
{
    return JSValue::encode(JSTestNode::serialize(*state, *thisObject, *thisObject->globalObject(), throwScope));
}

EncodedJSValue JSC_HOST_CALL jsTestNodePrototypeFunctionToJSON(ExecState* state)
{
    return IDLOperation<JSTestNode>::call<jsTestNodePrototypeFunctionToJSONBody>(*state, "toJSON");
}

#if ENABLE(BINDING_INTEGRITY)
#if PLATFORM(WIN)
#pragma warning(disable: 4483)
extern "C" { extern void (*const __identifier("??_7TestNode@WebCore@@6B@")[])(); }
#else
extern "C" { extern void* _ZTVN7WebCore8TestNodeE[]; }
#endif
#endif

JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject* globalObject, Ref<TestNode>&& impl)
{

#if ENABLE(BINDING_INTEGRITY)
    void* actualVTablePointer = *(reinterpret_cast<void**>(impl.ptr()));
#if PLATFORM(WIN)
    void* expectedVTablePointer = WTF_PREPARE_VTBL_POINTER_FOR_INSPECTION(__identifier("??_7TestNode@WebCore@@6B@"));
#else
    void* expectedVTablePointer = WTF_PREPARE_VTBL_POINTER_FOR_INSPECTION(&_ZTVN7WebCore8TestNodeE[2]);
#endif

    // If this fails TestNode does not have a vtable, so you need to add the
    // ImplementationLacksVTable attribute to the interface definition
    static_assert(std::is_polymorphic<TestNode>::value, "TestNode is not polymorphic");

    // If you hit this assertion you either have a use after free bug, or
    // TestNode has subclasses. If TestNode has subclasses that get passed
    // to toJS() we currently require TestNode you to opt out of binding hardening
    // by adding the SkipVTableValidation attribute to the interface IDL definition
    RELEASE_ASSERT(actualVTablePointer == expectedVTablePointer);
#endif
    return createWrapper<TestNode>(globalObject, WTFMove(impl));
}

JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, TestNode& impl)
{
    return wrap(state, globalObject, impl);
}


}
