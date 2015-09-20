#ifndef Statement_h
#define Statement_h

#include "CommonTypes.h"
#include "NameBindings.h"

class Context;

class Statement: public Node {
public:

    enum Kind {
        VarDeclaration,
        Block,
        ExpressionStatement,
        If,
        While,
        Break,
        Continue,
        Return,
        ConstructorCall,
        Label,
        Jump
    };

    Statement(Kind k, const Location& l);

    virtual Statement* clone() const = 0;
    virtual Type* typeCheck(Context& context) = 0;

    Kind getKind() const {
        return kind;
    }

    bool isExpression() const {
        return kind == ExpressionStatement;
    }

private:
    Kind kind;
};

class VariableDeclarationStatement: public Statement {
public:
    VariableDeclarationStatement(
        Type* t,
        const Identifier& i,
        Expression* e,
        const Location& l);
    VariableDeclarationStatement(
        Type* t,
        Expression* p,
        Expression* e,
        const Location& l);
    VariableDeclarationStatement(const Identifier& i, Expression* e);
    VariableDeclarationStatement(const VariableDeclarationStatement& other);

    virtual VariableDeclarationStatement* clone() const;
    virtual Type* typeCheck(Context& context);

    static Identifier generateTemporaryName(const Identifier& name);
    static VariableDeclarationStatement* generateTemporary(
        Type* t,
        const Identifier& name,
        Expression* init,
        const Location& loc);

    void setInitExpression(Expression* e) {
        initExpression = e;
    }

    void setIsNameUnique(bool u) {
        isNameUnique = u;
    }

    void setAddToNameBindingsWhenTypeChecked(bool a) {
        addToNameBindingsWhenTypeChecked = a;
    }

    Type* getType() const {
        return declaration.getType();
    }

    const Identifier& getIdentifier() const {
        return declaration.getIdentifier();
    }

    VariableDeclaration* getDeclaration() {
        return &declaration;
    }

    Expression* getInitExpression() const {
        return initExpression;
    }

    bool hasPattern() const {
        return patternExpression != nullptr;
    }

    bool getAddToNameBindingsWhenTypeChecked() const {
        return addToNameBindingsWhenTypeChecked;
    }

private:
    void processInitExpression(Context& context);
    void generateDeclarationsFromPattern(Context& context);
    Expression* generateInitTemporary(Context& context);
    void changeTypeIfGeneric(Context& context);
    void makeIdentifierUniqueIfTakingLambda(Context& context);

    VariableDeclaration declaration;
    Expression* patternExpression;
    Expression* initExpression;
    bool isNameUnique;
    bool addToNameBindingsWhenTypeChecked;
};

typedef std::vector<VariableDeclarationStatement*>
    VariableDeclarationStatementList;

class LabelStatement;
class ConstructorCallStatement;

class BlockStatement: public Statement {
public:
    BlockStatement(
        ClassDefinition* classDef,
        BlockStatement* enclosing,
        const Location& l);
    BlockStatement(const BlockStatement& other);

    virtual BlockStatement* clone() const;
    virtual Type* typeCheck(Context& context);

    void addStatement(Statement* statement);
    void insertStatementAtFront(Statement* statement);
    void insertStatementAfterFront(Statement* statement);
    void insertBeforeCurrentStatement(Statement* statement);
    void copyStatements(const BlockStatement& from);
    void addLocalBinding(VariableDeclaration* localObject);
    void setEnclosingBlock(BlockStatement* b);
    void returnLastExpression(
        VariableDeclarationStatement* retvalTmpDeclaration);
    ConstructorCallStatement* getFirstStatementAsConstructorCall() const;
    void replaceLastStatement(Statement* statement);
    void replaceCurrentStatement(Statement* s);
    Expression* getLastStatementAsExpression() const;

    typedef std::list<Statement*> StatementList;

    StatementList& getStatements() {
        return statements;
    }

    const StatementList& getStatements() const {
        return statements;
    }

    NameBindings& getNameBindings() {
        return nameBindings;
    }

    BlockStatement* getEnclosingBlock() const {
        return enclosingBlock;
    }

private:
    void initialStatementCheck(Statement* statement);
    void addLabel(const LabelStatement* label);

    NameBindings nameBindings;
    StatementList statements;
    StatementList::iterator iterator;
    BlockStatement* enclosingBlock;
};

class IfStatement: public Statement {
public:
    IfStatement(
        Expression* e,
        BlockStatement* b,
        BlockStatement* eb,
        const Location& l);

    virtual Statement* clone() const;
    virtual Type* typeCheck(Context& context);

    Expression* getExpression() const {
        return expression;
    }

    BlockStatement* getBlock() const {
        return block;
    }

    BlockStatement* getElseBlock() const {
        return elseBlock;
    }

private:
    Expression* expression;
    BlockStatement* block;
    BlockStatement* elseBlock;
};

class WhileStatement: public Statement {
public:
    WhileStatement(Expression* e, BlockStatement* b, const Location& l);

    virtual Statement* clone() const;
    virtual Type* typeCheck(Context& context);

    Expression* getExpression() const {
        return expression;
    }

    BlockStatement* getBlock() const {
        return block;
    }

private:
    Expression* expression;
    BlockStatement* block;
};

class BreakStatement: public Statement {
public:
    explicit BreakStatement(const Location& l);

    virtual Statement* clone() const;
    virtual Type* typeCheck(Context& context);
};

class ContinueStatement: public Statement {
public:
    explicit ContinueStatement(const Location& l);

    virtual Statement* clone() const;
    virtual Type* typeCheck(Context& context);
};

class ReturnStatement: public Statement {
public:
    ReturnStatement(Expression* e, const Location& l);
    explicit ReturnStatement(Expression* e);
    ReturnStatement(const ReturnStatement& other);

    virtual Statement* clone() const;
    virtual Type* typeCheck(Context& context);

    Expression* getExpression() const {
        return expression;
    }

private:
    Expression* expression;
    MethodDefinition* originalMethod;
};

class MethodCallExpression;

class ConstructorCallStatement: public Statement {
public:
    ConstructorCallStatement(MethodCallExpression* c);

    virtual Statement* clone() const;
    virtual Type* typeCheck(Context& context);

    MethodCallExpression* getMethodCallExpression() const {
        return constructorCall;
    }

    bool isBaseClassConstructorCall() const {
        return isBaseClassCtorCall;
    }

private:
    MethodCallExpression* constructorCall;
    bool isBaseClassCtorCall;
};

class LabelStatement: public Statement {
public:
    LabelStatement(const Identifier& n, const Location& l);

    virtual Statement* clone() const;
    virtual Type* typeCheck(Context&);

    const Identifier& getName() const {
        return name;
    }

private:
    Identifier name;
};

class JumpStatement: public Statement {
public:
    JumpStatement(const Identifier& n, const Location& l);

    virtual Statement* clone() const;
    virtual Type* typeCheck(Context& context);

    const Identifier& getLabelName() const {
        return labelName;
    }

private:
    Identifier labelName;
};

#endif