#include <assert.h>

#include "NameBindings.h"
#include "Definition.h"

Binding::Binding(ReferencedEntity e) :
    referencedEntity(e),
    definition(nullptr),
    localObject(nullptr),
    methodList() {}

Binding::Binding(ReferencedEntity e, VariableDeclaration* o) :
    referencedEntity(e),
    definition(nullptr),
    localObject(o),
    methodList() {}

Binding::Binding(ReferencedEntity e, Definition* d) : 
    referencedEntity(e),
    definition(d),
    localObject(nullptr),
    methodList() {}

Binding::Binding(ReferencedEntity e, MethodDefinition* d) : 
    referencedEntity(e),
    definition(nullptr),
    localObject(nullptr),
    methodList() {

    methodList.push_back(d);
}

Binding::Binding(const Binding& other) :
    referencedEntity(other.referencedEntity),
    definition(other.definition),
    localObject(other.localObject),
    methodList(other.methodList) {}

bool Binding::isReferencingType() const {
    switch (referencedEntity) {
        case Class:
        case GenericTypeParameter:
            return true;
        default:
            return false;
    }
}

NameBindings::NameBindings() : enclosing(nullptr) {}

NameBindings::NameBindings(NameBindings* enc) : enclosing(enc) {}

void NameBindings::copyFrom(const NameBindings& from) {
    const BindingMap& fromBindings = from.bindings;
    for (BindingMap::const_iterator i = fromBindings.begin();
         i != fromBindings.end();
         i++) {
        bindings.insert(make_pair(i->first, new Binding(*(i->second))));
    }
}

void NameBindings::use(const NameBindings& usedNamespace) {
    const BindingMap& usedBindings = usedNamespace.bindings;
    for (BindingMap::const_iterator i = usedBindings.begin();
         i != usedBindings.end();
         i++) {
        Binding* binding = i->second;
        switch (binding->getReferencedEntity()) {
            case Binding::Class:
            case Binding::Method:
            case Binding::DataMember:
                bindings.insert(make_pair(i->first, new Binding(*binding)));
                break;
            default:
                break;
        }
    }
}

Binding* NameBindings::lookup(const Identifier& name) const {
    BindingMap::const_iterator i = bindings.find(name);
    if (i == bindings.end()) {
        if (enclosing != nullptr) {
            return enclosing->lookup(name);
        }
        return nullptr;
    }
    return i->second;
}

Definition* NameBindings::lookupType(const Identifier& name) const {
    BindingMap::const_iterator i = bindings.find(name);
    if (i == bindings.end() || !(i->second->isReferencingType())) {
        if (enclosing != nullptr) {
            return enclosing->lookupType(name);
        }
        return nullptr;
    }
    return i->second->getDefinition();
}

Binding* NameBindings::lookupLocal(const Identifier& name) const {
    BindingMap::const_iterator i = bindings.find(name);
    if (i == bindings.end()) {
        return nullptr;
    }
    return i->second;
}

bool NameBindings::insertLocalObject(VariableDeclaration* localObject) {
    Binding* binding = new Binding(Binding::LocalObject, localObject);
    return bindings.insert(make_pair(localObject->getIdentifier(),
                                     binding)).second;
}

void NameBindings::removeObsoleteLocalBindings() {
    for (BindingMap::const_iterator i = bindings.begin();
         i != bindings.end(); ) {
        Binding* binding = i->second;
        const Identifier& nameInBindings = i->first;
        if (binding->getReferencedEntity() == Binding::LocalObject &&
            nameInBindings.compare(
                binding->getLocalObject()->getIdentifier()) != 0) {
            bindings.erase(i++);
            delete binding;
        } else {
            ++i;
        }
    }
}

bool NameBindings::insertClass(
    const Identifier& name,
    ClassDefinition* classDef) {

    Binding* binding = new Binding(Binding::Class, classDef);
    return bindings.insert(make_pair(name, binding)).second;
}

bool NameBindings::insertDataMember(
    const Identifier& name,
    DataMemberDefinition* dataMemberDef) {

    Binding* binding = new Binding(Binding::DataMember, dataMemberDef);
    return bindings.insert(make_pair(name, binding)).second;
}

bool NameBindings::removeDataMember(const Identifier& name) {
    Binding* binding = lookupLocal(name);
    if (binding == nullptr ||
        binding->getReferencedEntity() != Binding::DataMember) {
        return false;
    }
    bindings.erase(name);
    return true;
}

bool NameBindings::insertMethod(
    const Identifier& name,
    MethodDefinition* methodDef) {

    Binding* binding = new Binding(Binding::Method, methodDef);
    return bindings.insert(make_pair(name, binding)).second;
}

bool NameBindings::overloadMethod(
    const Identifier& name,
    MethodDefinition* methodDef) {

    Binding* binding = lookupLocal(name);
    if (binding == nullptr) {
        return insertMethod(name, methodDef);
    } else if (binding->getReferencedEntity() != Binding::Method) {
        return false;
    } else {
        binding->getMethodList().push_back(methodDef);
        return true;
    }
}

bool NameBindings::updateMethodName(
    const Identifier& oldName,
    const Identifier& newName) {

    Binding* binding = lookupLocal(oldName);
    if (binding == nullptr ||
        binding->getReferencedEntity() != Binding::Method) {
        return false;
    }
    bindings.erase(oldName);
    return bindings.insert(make_pair(newName, binding)).second;
}

bool NameBindings::removeLastOverloadedMethod(const Identifier& name) {
    Binding* binding = lookupLocal(name);
    if (binding == nullptr ||
        binding->getReferencedEntity() != Binding::Method) {
        return false;
    }
    binding->getMethodList().pop_back();
    return true;
}

bool NameBindings::insertGenericTypeParameter(
    const Identifier& name,
    GenericTypeParameterDefinition* genericTypeParameterDef) {

    Binding* binding = new Binding(Binding::GenericTypeParameter,
                                   genericTypeParameterDef);
    return bindings.insert(make_pair(name, binding)).second;
}

bool NameBindings::insertLabel(const Identifier& label) {
    if (lookup(label) != nullptr) {
        return false;
    }
    Binding* binding = new Binding(Binding::Label);
    bindings.insert(make_pair(label, binding));
    return true;
}