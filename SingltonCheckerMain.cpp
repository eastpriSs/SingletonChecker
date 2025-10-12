#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;


namespace AnalysisAlgorithm 
{
        template<typename T>
        bool isClassObject(T* varDecl, CXXRecordDecl* clssDecl)
        {
            if (!(varDecl && clssDecl)) return false;
            return !varDecl->getType()->isPointerType() && !varDecl->getType()->isReferenceType()
                && (varDecl->getType()->getCanonicalTypeUnqualified() == clssDecl->getTypeForDecl()->getCanonicalTypeUnqualified());
        }

        void findAssignmentsInStmt(Stmt* stmt, std::vector<BinaryOperator*>& assignments) 
        {
            if (!stmt) return;
            
            if (auto* binOp = dyn_cast<BinaryOperator>(stmt)) {
                if (binOp->getOpcode() == BO_Assign) {
                    assignments.push_back(binOp);
                }
            }
            
            // Рекурсивно обходим дочерние statements
            for (Stmt* child : stmt->children()) {
                findAssignmentsInStmt(child, assignments);
            }
        }

        int countClassStaticObject(CXXRecordDecl* clssDecl, FunctionDecl* funcDecl)
        {
            if (!funcDecl->hasBody() || !(clssDecl && funcDecl)) return 0;
            
            int count = 0;
            for (Stmt* st : funcDecl->getBody()->children()) {
                if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                    for (Decl* dcl : declStmt->decls()) {
                        if (VarDecl* varDecl = dyn_cast<VarDecl>(dcl)){
                            if (isClassObject(varDecl, clssDecl) && varDecl->isStaticLocal()) {
                                ++count;
                            }
                        }
                    }
                }
            }
            return count;
        }

        int countClassStaticObject(CXXRecordDecl* clssDecl, CXXRecordDecl* targetClssDecl)
        {
            if (!(clssDecl && targetClssDecl)) return 0;
            
            int count = 0;
            for (auto* field : targetClssDecl->decls()) 
                if (isClassObject(dyn_cast<VarDecl>(field), clssDecl))
                   ++count;
            
            for (auto* method : targetClssDecl->methods()) 
                count += countClassStaticObject(clssDecl, method);
            
            return count; 
        }

        bool findClassLocalObject(CXXRecordDecl* clssDecl, CXXRecordDecl* targetClssDecl)
        {
            if (!(clssDecl && targetClssDecl)) return 0;
           
            for (auto* field : targetClssDecl->fields()) {
                if (field->getType()->isPointerType() || field->getType()->isReferenceType())
                   continue;
                else if (isClassObject(field, clssDecl))
                    return true;
            }
            return false;
        }

        bool findClassLocalObject(CXXRecordDecl* clssDecl, FunctionDecl* funcDecl)
        {
            if (!funcDecl->hasBody() || !(clssDecl && funcDecl)) return false;
            
            for (Stmt* st : funcDecl->getBody()->children()) {
                if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                    for (Decl* dcl : declStmt->decls()) {
                        if (VarDecl* varDecl = dyn_cast<VarDecl>(dcl)){
                            if (isClassObject(varDecl, clssDecl) && !varDecl->isStaticLocal()) {
                               return true;
                            }
                        }
                    }
                }
            }
            return false;
        }

        bool isSameType(QualType type1, QualType type2) {
          return type1.getTypePtr()->getUnqualifiedDesugaredType() == 
                 type2.getTypePtr()->getUnqualifiedDesugaredType();
        }


        bool compareReturnTypeWithRecordType(FunctionDecl *method, CXXRecordDecl *record) {
            QualType returnType = method->getReturnType();
            QualType recordType = record->getASTContext().getRecordType(record);
            
            if (returnType->isPointerType() || returnType->isReferenceType())
                returnType = returnType->getPointeeType();

            return isSameType(returnType, recordType);
        }

        template<typename Op1, typename Op2>
        bool isEQNEBetween(BinaryOperator* bo) 
        {
           if (!bo) return false; 
           if (bo->getOpcode() != BO_EQ && bo->getOpcode() != BO_NE)
               return false;
           return isa<Op1>(bo->getLHS()->IgnoreImpCasts()) && isa<Op2>(bo->getRHS()->IgnoreImpCasts());
        }

        VarDecl* getVarDeclFromExpr(Expr* expr) {
            expr = expr->IgnoreParenCasts();
            if (auto *declRef = dyn_cast<DeclRefExpr>(expr)) {
                return dyn_cast<VarDecl>(declRef->getDecl());
            }
            return nullptr;
        }
 
};


namespace {


class FunctionVisitor : public RecursiveASTVisitor<FunctionVisitor> {
private:
    ASTContext *Context;

public:
    explicit FunctionVisitor(ASTContext *Context) : Context(Context) {}

    bool VisitFunctionDecl(FunctionDecl *func) {
        if (isa<CXXMethodDecl>(func)) {
            return true;
        }
        
        if (!func->hasBody() || !func->getBody()) {
            return true;
        }
        
        if (!func->getReturnType()->isPointerType()) {
            return true;
        }
        
        auto returnType = func->getReturnType()->getPointeeType();
        
        for (Stmt* st : func->getBody()->children()) {
            if (!st) continue;
            
            if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                for (Decl *decl : declStmt->decls()) {
                    if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
                        if (varDecl->isStaticLocal()) {
                        }
                    }
                }
            }
            
            if (auto *retStmt = dyn_cast<ReturnStmt>(st)) {
                Expr* retExpr = retStmt->getRetValue();
                if (!retExpr) continue;
                
                if (auto *declRef = dyn_cast<DeclRefExpr>(retExpr)) {
                    ValueDecl *valueDecl = declRef->getDecl();
                    if (!valueDecl) continue;
                    
                    if (VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl)) {
                        if (varDecl->isStaticLocal() && 
                            varDecl->getType()->getCanonicalTypeUnqualified() == returnType) {
                        }
                    }
                }
            }
        }
        return true;
    }
};



class ClassVisitor : public RecursiveASTVisitor<ClassVisitor> {
private:
    ASTContext *Context;
    SourceManager* SM;

    struct AnalysisData {
        bool ctorsPrivate                   : 1; 
        bool hasMethodLikelyInstance        : 1; 
        bool hasFriendFunctionLikelyInstance: 1; 
        bool hasDeletedCopyConstuctor       : 1; 
        bool hasDeletedAssigmentOperator    : 1;
        bool notSingleton                   : 1;
        bool hiddenInstanceMethod           : 1;
        bool probabalyNaiveSingletone       : 1;
        bool probabalyCRTPSingletone        : 1;
        bool probablyMayersSingletone       : 1;
        unsigned int amountObjects          : 27;
        
        enum ConditionPatternInGetInstance {
            UnaryOperatorInCondition,
            BinaryOperatorInConditionNullptr,
            BinaryOperatorInConditionNull,
            VarInCondition,
            UnknownCondition,
        } conditionPatternInGetInstance;

        CXXMethodDecl* methodLikeGetInstance         = nullptr;      
        FunctionDecl* friendFunctionLikeGetInstance  = nullptr;      
        VarDecl* instanceField                       = nullptr;
        std::string className;
        std::string location;
        
        inline void  clear() noexcept
        {
            probabalyCRTPSingletone = false;
            methodLikeGetInstance = nullptr;      
            friendFunctionLikeGetInstance = nullptr;
            instanceField = nullptr;
            hiddenInstanceMethod = false;
            ctorsPrivate = true;
            hasMethodLikelyInstance = false;
            hasDeletedCopyConstuctor = false;
            hasDeletedAssigmentOperator = false;
            notSingleton = false;
            amountObjects = 0;
            probabalyNaiveSingletone = false;
            hasFriendFunctionLikelyInstance = false;
            className.clear();
            location.clear();
            
        }

        inline void dump() const noexcept  
        {
            const int totalWidth = 76;
            const int labelWidth = 55;
            
            auto printLine = [](const std::string& text) {
                llvm::outs() << "║ " << text << "\n";
            };
            
            auto printField = [&](const std::string& label, const std::string& value) {
                std::string line = "|   • " + label + ":";
                line.resize(labelWidth, ' ');
                line += value;
                printLine(line);
            };
            
            llvm::outs() << "\n";
            llvm::outs() << "╔══════════════════════════════════════════════════════════════════╗\n";
            llvm::outs() << "║                     CLASS ANALYSIS REPORT                        ║\n";
            llvm::outs() << "╠══════════════════════════════════════════════════════════════════╣\n";
            
            // Class info
            printLine("Class:    " + className);
            printLine("Location: " + location);
            llvm::outs() << "╠══════════════════════════════════════════════════════════════════╣\n";
            printLine("Singleton Pattern Analysis:");
            
            // Analysis fields
            printField("Constructors private",        ctorsPrivate ? " ✓ YES" : "✗ NO");
            printField("GetInstance method",          hasMethodLikelyInstance ? " ✓ FOUND" : "✗ NOT FOUND");
            
            if (hasMethodLikelyInstance) {
                printField("GetInstance access",        hiddenInstanceMethod ? " ✗ HIDDEN" : "✓ PUBLIC");
            }
            
            printField("Friend function like getInstance()", hasFriendFunctionLikelyInstance ? " ✓ YES" : "✗ NO");
            printField("Copy constructor deleted",    hasDeletedCopyConstuctor ? " ✓ YES" : "✗ NO");
            printField("Assignment operator deleted", hasDeletedAssigmentOperator ? " ✓ YES" : "✗ NO");
            printField("Static instances count",      std::to_string(amountObjects));
            printField("Probably naive singletone",      probabalyNaiveSingletone ? " ✓ YES" : "✗ NO");
            printField("Probably Mayer's singletone",   probablyMayersSingletone ? " ✓ YES" : "✗ NO");
            printField("Multiple instances detected", notSingleton ? "  YES" : " NO");
            
            // Final conclusion
            llvm::outs() << "╠══════════════════════════════════════════════════════════════════╣\n";
            
            
            std::string conclusion = "Conclusion: " + std::string(notSingleton ? " ✓ LIKELY SINGLETON" : "✗ NOT A SINGLETON");
            printLine(conclusion);
            llvm::outs() << "╚══════════════════════════════════════════════════════════════════╝\n";
            llvm::outs() << "\n";
            
        }

    } analysisData;


    template<typename T1, typename T2>
    struct AnalysisPair 
    {
        T1 extracted;
        T2 param;
    };

    AnalysisPair<VarDecl*, AnalysisData::ConditionPatternInGetInstance> analysisCondition(Expr* ce)
    {
        using AnalysisAlgorithm::getVarDeclFromExpr;
        if (!ce) return {nullptr, AnalysisData::UnknownCondition};
        
        Expr* clearCE = ce->IgnoreParenImpCasts();
        
        // instance
        if (VarDecl* var = getVarDeclFromExpr(clearCE))
            return {var, AnalysisData::VarInCondition};
        
        //  !instance
        if (auto* unOp = dyn_cast<UnaryOperator>(clearCE)) {
            if (unOp->getOpcode() == UO_LNot) {
                Expr* subExpr = unOp->getSubExpr()->IgnoreParenImpCasts();
                    return {getVarDeclFromExpr(subExpr), AnalysisData::UnaryOperatorInCondition};
            }
        }
        
        // (instance == nullptr, nullptr == instance и т.д.)
        if (auto* binOp = dyn_cast<BinaryOperator>(clearCE)) {
            if (binOp->getOpcode() == BO_EQ || binOp->getOpcode() == BO_NE) {
                Expr* lhs = binOp->getLHS()->IgnoreParenImpCasts();
                Expr* rhs = binOp->getRHS()->IgnoreParenImpCasts();
                
                auto checkNullComparison = [&](Expr* varSide, Expr* nullSide) -> VarDecl* {
                    if (VarDecl* var = getVarDeclFromExpr(varSide))
                        if (isa<CXXNullPtrLiteralExpr>(nullSide) 
                        ||  isa<GNUNullExpr>(nullSide)) 
                                return var;
                    return nullptr;
                };
                
                if (VarDecl* var = checkNullComparison(lhs, rhs)) {
                    return {var, AnalysisData::BinaryOperatorInConditionNullptr};
                }
                if (VarDecl* var = checkNullComparison(rhs, lhs)) {
                    return {var, AnalysisData::BinaryOperatorInConditionNullptr};
                }
            }
        }
        
        //  (instance ? ... : ...)
        if (auto* condOp = dyn_cast<ConditionalOperator>(clearCE)) 
            return analysisCondition(condOp->getCond());
        return {nullptr, AnalysisData::UnknownCondition};
    }

private:

        void registerClassForAnalysisData(CXXRecordDecl* clsAST) 
        {
            analysisData.className = clsAST->getNameAsString();
            analysisData.location = clsAST->getLocation().printToString(Context->getSourceManager());
        }

     
        bool isProbablyGetInstanceFunction(FunctionDecl *method) 
        {  
            using AnalysisAlgorithm::getVarDeclFromExpr;
            using AnalysisAlgorithm::findAssignmentsInStmt;

            if (!method || !method->hasBody()) return false;
            
            if (!(method->getReturnType()->isPointerType() ||  
                  method->getReturnType()->isReferenceType())) {
                return false;
            }

            for (Stmt* stmt : method->getBody()->children()) {
                if (!stmt) continue;
                
                // Return statements
                if (auto *retStmt = dyn_cast<ReturnStmt>(stmt)) {
                    Expr* retExpr = retStmt->getRetValue();
                    if (!retExpr) continue;
                    
                    retExpr = retExpr->IgnoreParenImpCasts();
                    VarDecl* returnedVar = getVarDeclFromExpr(retExpr);
                   
                    // return *instance / return &instance
                    if (auto *unop = dyn_cast<UnaryOperator>(retExpr)){
                        if (unop->getOpcode() != UO_AddrOf && 
                            unop->getOpcode() != UO_Deref) {
                            continue;
                        }
                        returnedVar = getVarDeclFromExpr(unop->getSubExpr()->IgnoreParenCasts()); 
                    }

                    if (returnedVar) {
                        // Mayer's pattern
                        if (returnedVar->isStaticLocal()) {
                            analysisData.instanceField = returnedVar;
                            analysisData.probablyMayersSingletone = true;
                        }
                    }
                }
                
                // Naive Singletone
                else if (auto* ifStmt = dyn_cast<IfStmt>(stmt)) {
                    Expr* condition = ifStmt->getCond();
                    if (!condition) continue;
                    
                    auto conditionResult = analysisCondition(condition);
                    VarDecl* conditionVar = conditionResult.extracted;
                    
                    if (conditionVar) {
                        Stmt* thenBody = ifStmt->getThen();
                        if (thenBody) {
                            std::vector<BinaryOperator*> assignments;
                            findAssignmentsInStmt(thenBody, assignments);
                            
                            for (auto* assign : assignments) {
                                if (assign->getOpcode() == BO_Assign) {
                                    VarDecl* assignedVar = getVarDeclFromExpr(assign->getLHS());
                                    if (assignedVar && assignedVar == conditionVar) {
                                        if (assignedVar->isStaticDataMember() && 
                                            assignedVar->getAccess() == AS_private) {
                                            analysisData.instanceField = assignedVar;
                                            analysisData.probabalyNaiveSingletone = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                //  (... ? true_expr : false_expr)
                else if (auto* condOp = dyn_cast<ConditionalOperator>(stmt)) {
                    Expr* condition = condOp->getCond();
                    Expr* trueExpr = condOp->getTrueExpr();
                    Expr* falseExpr = condOp->getFalseExpr();
                    
                    auto conditionResult = analysisCondition(condition);
                    VarDecl* conditionVar = conditionResult.extracted;
                    
                    if (conditionVar) {
                        VarDecl* trueVar = getVarDeclFromExpr(trueExpr);
                        VarDecl* falseVar = getVarDeclFromExpr(falseExpr);
                        
                        if (trueVar || falseVar) { 
                            if (conditionVar->isStaticLocal())
                                analysisData.probablyMayersSingletone = true;
                            else if (conditionVar->isStaticDataMember() && conditionVar->getAccess() == AS_private)
                                analysisData.probabalyNaiveSingletone = true;
                        }
                    }
                }
            }
            return analysisData.probabalyNaiveSingletone || analysisData.probablyMayersSingletone;
        }

        bool shouldSkipDeclaration(Decl *decl) 
        {
            if (!decl) return true;
            
            SourceLocation loc = decl->getLocation();
            if (!SM->isInMainFile(loc) && !SM->isWrittenInMainFile(loc)) {
                return true;
            }
            
            if (SM->isInSystemHeader(loc) || SM->isInSystemMacro(loc)) {
                return true;
            }
            
            return false;
        }

public:
    explicit ClassVisitor(ASTContext *Context) : Context(Context) {
        SM = &Context->getSourceManager();
    }

    bool VisitCXXRecordDecl(CXXRecordDecl *declaration) {
       
        using namespace AnalysisAlgorithm;

        if (shouldSkipDeclaration(declaration))
            return true;

        if (declaration->isEmbeddedInDeclarator() && !declaration->isFreeStanding()) {
            return true;
        }

        if ( declaration->isInjectedClassName() ||
            declaration->isLambda()) {
            return true;
        }

        /*
        if (declaration->getFriendObjectKind() != Decl::FOK_None) {
            return true;
        }
        */
        
        analysisData.clear();
        registerClassForAnalysisData(declaration);

       
        // first stage of analysis
        for (const auto* c : declaration->ctors()) {
            if (c->getAccess() == AS_public && !c->isDeleted()) {
               analysisData.ctorsPrivate = false; 
               break;
            }
        }
        
        if (!analysisData.ctorsPrivate)
            return true;
        
        analysisData.hasDeletedCopyConstuctor = true;
        analysisData.hasDeletedAssigmentOperator = true;
        for (auto *method : declaration->methods()) {
            if (method->isStatic() 
            && compareReturnTypeWithRecordType(method, declaration)) {
                analysisData.hasMethodLikelyInstance = isProbablyGetInstanceFunction(method); 
                if (analysisData.hasMethodLikelyInstance && (method->getAccess() != AS_public))
                    analysisData.hiddenInstanceMethod = true;
            }
                
            if (CXXConstructorDecl* ctrDecl = dyn_cast<CXXConstructorDecl>(method))
            {
                if (ctrDecl->isCopyOrMoveConstructor()) 
                    analysisData.hasDeletedCopyConstuctor &= ctrDecl->isDeleted();
            }

            if (method->isCopyAssignmentOperator())
                analysisData.hasDeletedAssigmentOperator &= method->isDeleted();
        
            // second stage of analysis
            analysisData.amountObjects += countClassStaticObject(declaration, method);
            if (analysisData.amountObjects > 1 || findClassLocalObject(declaration, method))
                analysisData.notSingleton = true;
        }

        // second stage of analysis  
        if (!analysisData.notSingleton) {
            for (auto* field : declaration->decls()) {
                if (isClassObject(dyn_cast<VarDecl>(field), declaration)) { 
                    if (++analysisData.amountObjects > 1) {
                        analysisData.notSingleton = true;
                        break;
                    }
                }
            }
        }

        // third stage of analysis
        if (!analysisData.notSingleton) {
            for (auto it = declaration->friend_begin(); it != declaration->friend_end(); ++it) {
                if (FriendDecl* friendDecl = *it) {
                    if (NamedDecl* nd = friendDecl->getFriendDecl()) {
                        if (FunctionDecl* funcFriend = dyn_cast<FunctionDecl>(nd)) {
                            if (compareReturnTypeWithRecordType(funcFriend, declaration)) {
                                analysisData.hasFriendFunctionLikelyInstance = isProbablyGetInstanceFunction(funcFriend);
                                if (analysisData.hasFriendFunctionLikelyInstance) analysisData.friendFunctionLikeGetInstance = funcFriend;
                           }
                           analysisData.amountObjects += countClassStaticObject(declaration, funcFriend);
                           if (analysisData.amountObjects > 1 || findClassLocalObject(declaration, funcFriend)) {
                                analysisData.notSingleton = true;
                                break;
                           }
                        }
                        
                    }
                    else if (TypeSourceInfo* tsi = friendDecl->getFriendType()) {
                        QualType qt = tsi->getType();
                        if (const RecordType* rt = qt->getAs<RecordType>()) {
                            if (CXXRecordDecl* friendClss = dyn_cast<CXXRecordDecl>(rt->getDecl())) { 
                                if (ClassTemplateSpecializationDecl* ctsd = dyn_cast<ClassTemplateSpecializationDecl>(friendClss))
                                //    ctsd->get
                                analysisData.amountObjects += countClassStaticObject(declaration, friendClss);
                                if (analysisData.amountObjects > 1 || findClassLocalObject(declaration, friendClss)){
                                    analysisData.notSingleton = true;
                                    break;
                                }
                            }
                        }
                    }
                }    
            }
        }

        //declaration->dump();

        analysisData.dump();
        return true;
    }


};

class ClassVisitorASTConsumer : public ASTConsumer {
public:
    explicit ClassVisitorASTConsumer(ASTContext *Context) : ClassVisitor(Context), FuncVisitor(Context) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        ClassVisitor.TraverseDecl(Context.getTranslationUnitDecl());
        FuncVisitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    ClassVisitor ClassVisitor;
    FunctionVisitor FuncVisitor;
};

class ClassVisitorPlugin : public PluginASTAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   StringRef InFile) override {
        return std::make_unique<ClassVisitorASTConsumer>(&CI.getASTContext());
    }

    bool ParseArgs(const CompilerInstance &CI,
                  const std::vector<std::string> &args) override {
        for (const auto &Arg : args) {
            if (Arg == "-help") {
                PrintHelp(llvm::errs());
                return false;
            }
        }
        return true;
    }

    void PrintHelp(llvm::raw_ostream &ros) {
        ros << "Class visitor plugin\n";
        ros << "Prints information about classes and their methods\n";
    }
};

} // namespace

static FrontendPluginRegistry::Add<ClassVisitorPlugin>
    X("class-visitor", "Prints information about classes");
