#include "Pattern.h"
#include "Definition.h"
#include "Expression.h"
#include "Context.h"

namespace {
    const Identifier matchSubjectLengthName("__match_subject_length");
    const Identifier boolTrueCaseName("true");
    const Identifier boolFalseCaseName("false");

    bool patternExpressionCreatesVariable(
        NamedEntityExpression* patternExpression,
        Context& context) {

        return !patternExpression->isReferencingStaticDataMember(context);
    }

    bool memberPatternIsIrrefutable(
        Expression* memberPattern,
        Context& context) {

        if (memberPattern == nullptr || memberPattern->isPlaceholder()) {
            return true;
        }
        if (memberPattern->isNamedEntity()) {
            NamedEntityExpression* namedEntity =
                memberPattern->cast<NamedEntityExpression>();
            if (patternExpressionCreatesVariable(namedEntity, context)) {
                // The member pattern introduces a new variable. This is an
                // irrefutable pattern.
                return true;
            }
        }
        return false;
    }

    MemberSelectorExpression* generateMatchSubjectMemberSelector(
        const Expression* subject,
        Expression* memberName) {

        return MemberSelectorExpression::create(subject->clone(),
                                                memberName,
                                                memberName->getLocation());
    }

    MethodCallExpression* getConstructorCall(Expression* e, Context& context) {
        if (auto constructorCall = e->dynCast<MethodCallExpression>()) {
            constructorCall->tryResolveEnumConstructor(context);
            return constructorCall;
        } else if (auto nameExpr = e->dynCast<NamedEntityExpression>()) {
            return nameExpr->getCall(context, true);
        } else if (auto memberSelector =
                       e->dynCast<MemberSelectorExpression>()) {
            return memberSelector->getRhsCall(context);
        } else {
            return nullptr;
        }
    }

    ClassDecompositionExpression* createClassDecompositionExpr(
        const MethodCallExpression* constructorCall,
        Context& context);

    ClassDecompositionExpression* createClassDecompositionFromConstructorCall(
        const MethodCallExpression* constructorCall,
        Context& context) {

        auto classDecomposition =
            ClassDecompositionExpression::create(
                Type::create(constructorCall->getName()),
                constructorCall->getLocation());

        auto type = classDecomposition->typeCheck(context);
        auto classDef = type->getDefinition()->cast<ClassDefinition>();

        const DataMemberList& primaryCtorArgDataMembers =
            classDef->getPrimaryCtorArgDataMembers();
        const ExpressionList& constructorPatternArgs =
            constructorCall->getArguments();
        if (primaryCtorArgDataMembers.size() != constructorPatternArgs.size()) {
            Trace::error("Wrong number of arguments in constructor pattern.",
                         constructorCall);
        }

        auto j = primaryCtorArgDataMembers.cbegin();
        auto i = constructorPatternArgs.cbegin();
        while (i != constructorPatternArgs.end()) {
            Expression* patternExpr = *i;
            DataMemberDefinition* dataMember = *j;
            auto memberName =
                NamedEntityExpression::create(dataMember->getName(),
                                              patternExpr->getLocation());
            if (auto constructorCall =
                    getConstructorCall(patternExpr, context)) {
                patternExpr = createClassDecompositionExpr(constructorCall,
                                                           context);
            }
            classDecomposition->addMember(memberName, patternExpr);
            i++;
            j++;
        }

        return classDecomposition;
    }

    ClassDecompositionExpression* createClassDecompositionFromEnumCtorCall(
        const MethodCallExpression* enumConstructorCall,
        const MethodDefinition* enumConstructor,
        Context& context) {

        const auto enumDef = enumConstructor->getClass();

        auto classDecomposition =
            ClassDecompositionExpression::create(
                Type::create(enumDef->getName()),
                enumConstructorCall->getLocation());

        const Identifier& enumVariantName(enumConstructor->getName());
        classDecomposition->setEnumVariantName(enumVariantName);
        classDecomposition->typeCheck(context);

        const ExpressionList& constructorPatternArgs =
            enumConstructorCall->getArguments();
        if (enumConstructor->getArgumentList().size() !=
            constructorPatternArgs.size()) {
            Trace::error(
                "Wrong number of arguments in enum constructor pattern.",
                enumConstructorCall);
        }
        if (constructorPatternArgs.empty()) {
            return classDecomposition;
        }

        auto enumVariantDef =
            enumDef->getNestedClass(
                Symbol::makeEnumVariantClassName(enumVariantName));
        const DataMemberList& variantDataMembers =
            enumVariantDef->getPrimaryCtorArgDataMembers();
        assert(variantDataMembers.size() == constructorPatternArgs.size());

        auto j = variantDataMembers.cbegin();
        auto i = constructorPatternArgs.cbegin();
        while (i != constructorPatternArgs.end()) {
            auto patternExpr = *i;
            auto dataMember = *j;
            auto memberSelector =
                MemberSelectorExpression::create(
                    Symbol::makeEnumVariantDataName(enumVariantName),
                    dataMember->getName(),
                    patternExpr->getLocation());
            if (auto constructorCall =
                    getConstructorCall(patternExpr, context)) {
                patternExpr = createClassDecompositionExpr(constructorCall,
                                                           context);
            }
            classDecomposition->addMember(memberSelector, patternExpr);
            i++;
            j++;
        }

        return classDecomposition;
    }

    ClassDecompositionExpression* createClassDecompositionExpr(
        const MethodCallExpression* constructorCall,
        Context& context) {

        auto enumConstructor = constructorCall->getEnumCtorMethodDefinition();
        if (enumConstructor != nullptr) {
            return createClassDecompositionFromEnumCtorCall(constructorCall,
                                                            enumConstructor,
                                                            context);
        } else {
            return createClassDecompositionFromConstructorCall(constructorCall,
                                                               context);
        }
    }
}

MatchCoverage::MatchCoverage(const Type* subjectType) : notCoveredCases() {
    if (subjectType->isBoolean()) {
        notCoveredCases.insert(boolTrueCaseName);
        notCoveredCases.insert(boolFalseCaseName);
    } else if (subjectType->isEnumeration()) {
        auto subjectTypeDef = subjectType->getDefinition();
        assert(subjectTypeDef->isClass());
        auto subjectClass = subjectTypeDef->cast<ClassDefinition>();
        assert(subjectClass->isEnumeration());

        for (auto member: subjectClass->getMembers()) {
            if (auto method = member->dynCast<MethodDefinition>()) {
                if (method->isEnumConstructor()) {
                    notCoveredCases.insert(method->getName());
                }
            }
        }
    } else {
        notCoveredCases.insert("all");
    }
}

bool MatchCoverage::isCaseCovered(const Identifier& caseName) const {
    return (notCoveredCases.find(caseName) == notCoveredCases.end());
}

bool MatchCoverage::areAllCasesCovered() const {
    return notCoveredCases.empty();
}

void MatchCoverage::markCaseAsCovered(const Identifier& caseName) {
    notCoveredCases.erase(caseName);
}

Pattern::Pattern() : declarations(), temporaries() {}

Pattern::Pattern(const Pattern& other) : declarations(), temporaries() {
    cloneVariableDeclarations(other);
}

void Pattern::cloneVariableDeclarations(const Pattern& from) {
    Utils::cloneList(declarations, from.declarations);
    Utils::cloneList(temporaries, from.temporaries);
}

Pattern* Pattern::create(Expression* e, Context& context) {
    if (auto array = e->dynCast<ArrayLiteralExpression>()) {
        return new ArrayPattern(array);
    } else if (auto typed = e->dynCast<TypedExpression>()) {
        return new TypedPattern(typed);
    } else if (auto classDecomposition =
                   e->dynCast<ClassDecompositionExpression>()) {
        return new ClassDecompositionPattern(classDecomposition);
    } else if (auto constructorCall = getConstructorCall(e, context)) {
        return new ClassDecompositionPattern(
            createClassDecompositionExpr(constructorCall, context));
    } else {
        return new SimplePattern(e);
    }
}

SimplePattern::SimplePattern(Expression* e) : expression(e) {}

SimplePattern::SimplePattern(const SimplePattern& other) :
    expression(other.expression->clone()) {}

Pattern* SimplePattern::clone() const {
    return new SimplePattern(*this);
}

bool SimplePattern::isMatchExhaustive(
    const Expression* subject,
    MatchCoverage& coverage,
    bool isMatchGuardPresent,
    Context& context) {

    if (expression->isPlaceholder()) {
        return !isMatchGuardPresent;
    }

    auto boolLiteral = expression->dynCast<BooleanLiteralExpression>();
    if (boolLiteral != nullptr && subject->getType()->isBoolean()) {
        Identifier boolCaseName;
        if (boolLiteral->getValue() == true) {
            boolCaseName = boolTrueCaseName;
        } else {
            boolCaseName = boolFalseCaseName;
        }

        if (coverage.isCaseCovered(boolCaseName)) {
            Trace::error("Pattern is unreachable.", expression);
        }
        if (!isMatchGuardPresent) {
            coverage.markCaseAsCovered(boolCaseName);
            if (coverage.areAllCasesCovered()) {
                return true;
            }
        }
        return false;
    }

    auto namedEntity = expression->dynCast<NamedEntityExpression>();
    if (namedEntity == nullptr) {
        return false;
    }
    if (!isMatchGuardPresent) {
        if (namedEntity->isReferencingName(subject)) {
            // The pattern refers back to the subject. This is an irrefutable
            // pattern.
            return true;
        }
        if (patternExpressionCreatesVariable(namedEntity, context)) {
            // The member pattern introduces a new variable. This is an
            // irrefutable pattern.
            return true;
        }
    }
    return false;
}

BinaryExpression* SimplePattern::generateComparisonExpression(
    const Expression* subject,
    Context& context) {

    const Location& location = expression->getLocation();

    auto namedEntity = expression->dynCast<NamedEntityExpression>();
    if (namedEntity != nullptr &&
        patternExpressionCreatesVariable(namedEntity, context)) {
        // The pattern introduces a new variable. The variable will bind to the
        // value of the match subject.
        declarations.push_back(
            VariableDeclarationStatement::create(Type::create(Type::Implicit),
                                                 namedEntity->getIdentifier(),
                                                 subject->clone(),
                                                 location));
    }

    return BinaryExpression::create(Operator::Equal,
                                    subject->clone(),
                                    expression,
                                    location);
}

ArrayPattern::ArrayPattern(ArrayLiteralExpression* e) : array(e) {}

ArrayPattern::ArrayPattern(const ArrayPattern& other) :
    array(other.array->clone()) {}

Pattern* ArrayPattern::clone() const {
    return new ArrayPattern(*this);
}

bool ArrayPattern::isMatchExhaustive(
    const Expression*,
    MatchCoverage&,
    bool isMatchGuardPresent,
    Context&) {

    const ExpressionList& elements = array->getElements();
    if (elements.size() == 1) {
        Expression* element = *elements.begin();
        if (element->isWildcard()) {
            return !isMatchGuardPresent;
        }
    }
    return false;
}

BinaryExpression* ArrayPattern::generateComparisonExpression(
    const Expression* subject,
    Context& context) {

    auto comparison = generateLengthComparisonExpression();
    bool toTheRightOfWildcard = false;

    const ExpressionList& elements = array->getElements();
    for (auto i = elements.cbegin(); i != elements.cend(); i++) {
        auto element = *i;
        auto elementComparison =
            generateElementComparisonExpression(subject,
                                                i,
                                                context,
                                                toTheRightOfWildcard);
        if (elementComparison != nullptr) {
            comparison = BinaryExpression::create(Operator::LogicalAnd,
                                                  comparison,
                                                  elementComparison,
                                                  element->getLocation());
        }

        if (element->isWildcard()) {
            toTheRightOfWildcard = true;
        }
    }
    return comparison;
}

BinaryExpression* ArrayPattern::generateElementComparisonExpression(
    const Expression* subject,
    ExpressionList::const_iterator i,
    Context& context,
    bool toTheRightOfWildcard) {

    Expression* element = *i;
    BinaryExpression* elementComparison = nullptr;
    switch (element->getKind()) {
        case Expression::NamedEntity:
            elementComparison =
                generateNamedEntityElementComparisonExpression(
                    subject,
                    i,
                    context,
                    toTheRightOfWildcard);
            break;
        case Expression::Placeholder:
        case Expression::Wildcard:
            break;
        default:
            elementComparison = BinaryExpression::create(
                Operator::Equal,
                generateArraySubscriptExpression(subject,
                                                 i,
                                                 toTheRightOfWildcard),
                element,
                element->getLocation());
            break;
    }
    return elementComparison;
}

BinaryExpression* ArrayPattern::generateNamedEntityElementComparisonExpression(
    const Expression* subject,
    ExpressionList::const_iterator i,
    Context& context,
    bool toTheRightOfWildcard) {

    Expression* element = *i;
    auto namedEntity = element->dynCast<NamedEntityExpression>();
    assert(namedEntity != nullptr);

    if (patternExpressionCreatesVariable(namedEntity, context)) {
        // The pattern introduces a new variable. The variable will bind to the
        // value of the corresponding array element in the match subject.
        const Location& location = namedEntity->getLocation();
        auto matchSubjectElementExpression =
            generateArraySubscriptExpression(subject, i, toTheRightOfWildcard);
        declarations.push_back(
            VariableDeclarationStatement::create(Type::create(Type::Implicit),
                                                 namedEntity->getIdentifier(),
                                                 matchSubjectElementExpression,
                                                 location));
        return nullptr;
    }

    return BinaryExpression::create(
        Operator::Equal,
        generateArraySubscriptExpression(subject, i, toTheRightOfWildcard),
        element,
        element->getLocation());
}

ArraySubscriptExpression* ArrayPattern::generateArraySubscriptExpression(
    const Expression* subject,
    ExpressionList::const_iterator i,
    bool toTheRightOfWildcard) {

    Expression* element = *i;
    const Location& location = element->getLocation();
    Expression* indexExpression = nullptr;
    const ExpressionList& elements = array->getElements();
    if (toTheRightOfWildcard) {
        int reverseIndex = std::distance(i, elements.end());
        indexExpression = BinaryExpression::create(
            Operator::Subtraction,
            NamedEntityExpression::create(
                matchSubjectLengthName,
                location),
            IntegerLiteralExpression::create(reverseIndex,
                                             location),
            location);
    } else {
        indexExpression =
            IntegerLiteralExpression::create(std::distance(elements.begin(), i),
                                             location);
    }
    return ArraySubscriptExpression::create(subject->clone(), indexExpression);
}

BinaryExpression* ArrayPattern::generateLengthComparisonExpression() {
    int numberOfElements = 0;
    bool wildcardPresent = false;

    for (auto element: array->getElements()) {
        if (element->isWildcard()) {
            if (wildcardPresent) {
                Trace::error("Wilcard '..' can only be present once in an array"
                             " pattern.",
                             element);
            }
            wildcardPresent = true;
        } else {
            numberOfElements++;
        }
    }

    Operator::Kind op;
    if (wildcardPresent) {
        op = Operator::GreaterOrEqual;
    } else {
        op = Operator::Equal;
    }

    const Location& location = array->getLocation();
    return BinaryExpression::create(
        op,
        NamedEntityExpression::create(
            matchSubjectLengthName,
            location),
        IntegerLiteralExpression::create(numberOfElements,
                                         location),
        location);
}

VariableDeclarationStatement*
ArrayPattern::generateMatchSubjectLengthDeclaration(const Expression* subject) {
    const Location& location = subject->getLocation();
    auto arrayLengthSelector =
        MemberSelectorExpression::create(
            subject->clone(),
            NamedEntityExpression::create(BuiltInTypes::arrayLengthMethodName,
                                          location),
            location);
    return VariableDeclarationStatement::create(Type::create(Type::Integer),
                                                matchSubjectLengthName,
                                                arrayLengthSelector,
                                                location);
}

ClassDecompositionPattern::ClassDecompositionPattern(
    ClassDecompositionExpression* e) :
    classDecomposition(e) {}

ClassDecompositionPattern::ClassDecompositionPattern(
    const ClassDecompositionPattern& other) :
    classDecomposition(other.classDecomposition->clone()) {}

Pattern* ClassDecompositionPattern::clone() const {
    return new ClassDecompositionPattern(*this);
}

bool ClassDecompositionPattern::isMatchExhaustive(
    const Expression* subject,
    MatchCoverage& coverage,
    bool isMatchGuardPresent,
    Context& context) {

    auto classPatternType = classDecomposition->typeCheck(context);
    const Identifier& enumVariantName =
        classDecomposition->getEnumVariantName();

    if (!enumVariantName.empty()) {
        return isEnumMatchExhaustive(enumVariantName,
                                     subject,
                                     coverage,
                                     isMatchGuardPresent,
                                     classPatternType,
                                     context);
    }

    if (!Type::areEqualNoConstCheck(subject->getType(),
                                    classPatternType,
                                    false)) {
        return false;
    }

    if (!isMatchGuardPresent && areAllMemberPatternsIrrefutable(context)) {
        return true;
    }
    return false;
}

bool ClassDecompositionPattern::isEnumMatchExhaustive(
    const Identifier& enumVariantName,
    const Expression* subject,
    MatchCoverage& coverage,
    bool isMatchGuardPresent,
    Type* patternType,
    Context& context) {

    if (!Type::areEqualNoConstCheck(subject->getType(), patternType, false)) {
        Trace::error("Enum type in pattern must be the same as the match "
                     "subject type. Pattern type: " +
                     patternType->toString() +
                     ". Match subject type: " +
                     subject->getType()->toString(),
                     classDecomposition);
    }

    if (coverage.isCaseCovered(enumVariantName)) {
        Trace::error("Pattern is unreachable.", classDecomposition);
    }
    if (!isMatchGuardPresent && areAllMemberPatternsIrrefutable(context)) {
        coverage.markCaseAsCovered(enumVariantName);
        if (coverage.areAllCasesCovered()) {
            return true;
        }
    }
    return false;
}

bool ClassDecompositionPattern::areAllMemberPatternsIrrefutable(
    Context& context) {

    for (const auto& member: classDecomposition->getMembers()) {
        if (!memberPatternIsIrrefutable(member.patternExpr, context)) {
            return false;
        }
    }
    return true;
}

BinaryExpression* ClassDecompositionPattern::generateComparisonExpression(
    const Expression* subject,
    Context& context) {

    auto comparison = generateTypeComparisonExpression(&subject);

    for (const auto& member: classDecomposition->getMembers()) {
        if (memberPatternIsIrrefutable(member.patternExpr, context)) {
            generateVariableCreatedByMemberPattern(member, subject, context);
        } else {
            auto memberComparison =
                generateMemberComparisonExpression(subject, member, context);
            if (comparison == nullptr) {
                comparison = memberComparison;
            } else {
                comparison =
                    BinaryExpression::create(Operator::LogicalAnd,
                                             comparison,
                                             memberComparison,
                                             member.patternExpr->getLocation());
            }
        }
    }
    return comparison;
}

void ClassDecompositionPattern::generateVariableCreatedByMemberPattern(
    const ClassDecompositionExpression::Member& member,
    const Expression* subject,
    Context& context) {

    NamedEntityExpression* patternVar = nullptr;
    if (member.patternExpr == nullptr) {
        patternVar = member.nameExpr->dynCast<NamedEntityExpression>();
    } else {
        patternVar = member.patternExpr->dynCast<NamedEntityExpression>();
        if (patternVar != nullptr &&
            !patternExpressionCreatesVariable(patternVar, context)) {
            patternVar = nullptr;
        }
    }

    if (patternVar != nullptr) {
        auto matchSubjectMemberExpression =
            generateMatchSubjectMemberSelector(subject, member.nameExpr);
        declarations.push_back(
            VariableDeclarationStatement::create(Type::create(Type::Implicit),
                                                 patternVar->getIdentifier(),
                                                 matchSubjectMemberExpression,
                                                 patternVar->getLocation()));
    }
}

BinaryExpression* ClassDecompositionPattern::generateMemberComparisonExpression(
    const Expression* subject,
    const ClassDecompositionExpression::Member& member,
    Context& context) {

    BinaryExpression* comparisonExpression = nullptr;
    Expression* subjectMemberSelector =
        generateMatchSubjectMemberSelector(subject, member.nameExpr);
    auto patternExpr = member.patternExpr;
    if (auto classDecompositionExpr =
            patternExpr->dynCast<ClassDecompositionExpression>()) {
        auto classDecompositionPattern =
            new ClassDecompositionPattern(classDecompositionExpr);

        // The type of the subject expression needs to be calculated before
        // calling generateComparisonExpression().
        Context tmpContext(context);
        subjectMemberSelector = subjectMemberSelector->transform(tmpContext);
        subjectMemberSelector->typeCheck(tmpContext);

        comparisonExpression =
            classDecompositionPattern->generateComparisonExpression(
                subjectMemberSelector,
                context);
        cloneVariableDeclarations(*classDecompositionPattern);
    } else {
        comparisonExpression =
            BinaryExpression::create(Operator::Equal,
                                     subjectMemberSelector,
                                     patternExpr,
                                     patternExpr->getLocation());
    }
    return comparisonExpression;
}

BinaryExpression* ClassDecompositionPattern::generateTypeComparisonExpression(
    const Expression** subject) {

    auto originalSubject = *subject;
    const Identifier& enumVariantName =
        classDecomposition->getEnumVariantName();
    if (!enumVariantName.empty()) {
        return generateEnumVariantTagComparisonExpression(originalSubject,
                                                          enumVariantName);
    }

    auto classDecompositionType = classDecomposition->getType();
    if (Type::areEqualNoConstCheck(originalSubject->getType(),
                                   classDecompositionType,
                                   false)) {
        // No need to generate type comparison. The pattern type and subject
        // types are equal.
        return nullptr;
    }

    const Location& location = classDecomposition->getLocation();
    const Identifier
        castedSubjectName("__" + classDecompositionType->getName() + "_" +
                          originalSubject->generateVariableName());
    Type* castedSubjectType = classDecompositionType->clone();
    castedSubjectType->setConstant(false);
    temporaries.push_back(
        VariableDeclarationStatement::create(castedSubjectType,
                                             castedSubjectName,
                                             nullptr,
                                             location));
    auto typeCast =
        TypeCastExpression::create(castedSubjectType,
                                   originalSubject->clone(),
                                   location);
    auto castedSubject = LocalVariableExpression::create(castedSubjectType,
                                                         castedSubjectName,
                                                         location);
    *subject = castedSubject;
    return BinaryExpression::create(
        Operator::NotEqual,
        BinaryExpression::create(Operator::AssignmentExpression,
                                 castedSubject->clone(),
                                 typeCast,
                                 location),
        NullExpression::create(location),
        location);
}

BinaryExpression*
ClassDecompositionPattern::generateEnumVariantTagComparisonExpression(
    const Expression* subject,
    const Identifier& enumVariantName) {

    const Location& location = classDecomposition->getLocation();
    const Identifier& enumName = subject->getType()->getFullConstructedName();

    auto tagMember =
        MemberSelectorExpression::create(subject->clone(),
                                         NamedEntityExpression::create(
                                             CommonNames::enumTagVariableName,
                                             location),
                                         location);
    auto tagConstant =
        MemberSelectorExpression::create(NamedEntityExpression::create(enumName,
                                                                       location),
                                         NamedEntityExpression::create(
                                             Symbol::makeEnumVariantTagName(
                                                 enumVariantName),
                                             location),
                                         location);
    return BinaryExpression::create(Operator::Equal,
                                    tagMember,
                                    tagConstant,
                                    location);
}

TypedPattern::TypedPattern(TypedExpression* e) : typedExpression(e) {}

TypedPattern::TypedPattern(const TypedPattern& other) :
    typedExpression(other.typedExpression->clone()) {}

Pattern* TypedPattern::clone() const {
    return new TypedPattern(*this);
}

bool TypedPattern::isMatchExhaustive(
    const Expression* subject,
    MatchCoverage&,
    bool isMatchGuardPresent,
    Context& context) {

    auto targetType = typedExpression->typeCheck(context);
    if (Type::areEqualNoConstCheck(subject->getType(), targetType, false) &&
        !isMatchGuardPresent) {
        return true;
    }

    return false;
}

BinaryExpression* TypedPattern::generateComparisonExpression(
    const Expression* subject,
    Context&) {

    auto targetType = typedExpression->getType();
    const Location& location = typedExpression->getLocation();
    const Identifier
        castedSubjectName("__" + targetType->getName() + "_" +
                          subject->generateVariableName());
    auto castedSubjectType = targetType->clone();
    castedSubjectType->setConstant(false);
    temporaries.push_back(
        VariableDeclarationStatement::create(castedSubjectType,
                                             castedSubjectName,
                                             nullptr,
                                             location));
    auto typeCast =
        TypeCastExpression::create(castedSubjectType,
                                   subject->clone(),
                                   location);
    auto castedSubject =
        LocalVariableExpression::create(castedSubjectType,
                                        castedSubjectName,
                                        location);
    if (auto resultName = typedExpression->getResultName()->dynCast<
                NamedEntityExpression>()) {
        declarations.push_back(
            VariableDeclarationStatement::create(Type::create(Type::Implicit),
                                                 resultName->getIdentifier(),
                                                 castedSubject->clone(),
                                                 resultName->getLocation()));
    }
    return BinaryExpression::create(
        Operator::NotEqual,
        BinaryExpression::create(Operator::AssignmentExpression,
                                 castedSubject->clone(),
                                 typeCast,
                                 location),
        NullExpression::create(location),
        location);
}
