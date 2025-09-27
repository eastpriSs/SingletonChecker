#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

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

        template<typename T>
        bool isClassObject(T* varDecl, CXXRecordDecl* clssDecl)
        {
            if (!(varDecl && clssDecl)) return false;
            return !varDecl->getType()->isPointerType()
                && (varDecl->getType()->getCanonicalTypeUnqualified() == clssDecl->getTypeForDecl()->getCanonicalTypeUnqualified());
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
            
            llvm::outs() << "'\nCOunt:" << count << '\n'; 
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

        void registerClassForAnalysisData(CXXRecordDecl* clsAST) 
        {
            analysisData.className = clsAST->getNameAsString();
            analysisData.location = clsAST->getLocation().printToString(Context->getSourceManager());
        }

        template<typename Op1, typename Op2>
        bool isEqualBetween(BinaryOperator* bo) 
        {
           if (!bo) return false; 
           if (bo->getOpcode() != BO_EQ)
               return false;
           if (dyn_cast<Op1>(bo->getLHS()->IgnoreImpCasts()) && dyn_cast<Op2>(bo->getRHS()->IgnoreImpCasts()))
               return true;
           return false;
        }

        bool isProbablyGetInstanceFunction(FunctionDecl *method) 
        {  
           if (method && !method->hasBody()) return false;
           
           if (!(method->getReturnType()->isPointerType() ||  method->getReturnType()->isReferenceType()))
               return false;

           method->dump();

           for (Stmt* st : method->getBody()->children()) {
                if (auto *retStmt = dyn_cast<ReturnStmt>(st)) {
                    Expr* retExpr = retStmt->getRetValue();

                    retExpr =  retExpr->IgnoreParenCasts();

                    if (auto *unop = dyn_cast<UnaryOperator>(retExpr)){
                        if (unop->getOpcode() != UO_AddrOf && 
                            unop->getOpcode() != UO_Deref) {
                            continue;
                        }
                        retExpr = unop->getSubExpr()->IgnoreParenCasts(); 
                    }

                    if (auto *declRef = dyn_cast<DeclRefExpr>(retExpr)){
                        ValueDecl *valueDecl = declRef->getDecl();
                        if (VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl)) {
                            if (varDecl->isStaticLocal() 
                            || (varDecl->isStaticDataMember() && (varDecl->getAccess() == AS_private))) {
                                // if find instance field in IF Statement and return statement
                                if (analysisData.instanceField && analysisData.instanceField == varDecl){
                                    analysisData.probabalyNaiveSingletone = true;
                                }
                                analysisData.instanceField = varDecl;
                                return true;
                            }
                        }
                    }
                }
                else if (auto* ifSt = dyn_cast<IfStmt>(st)) {
                    Expr* se = ifSt->getCond()->IgnoreImpCasts();
                    
                    // Context:    !instance
                    if (auto* un = dyn_cast<UnaryOperator>(se)) {
                        if (un->getOpcode() == UO_LNot) {
                            se = un->getSubExpr()->IgnoreImpCasts();
                            analysisData.typeNaiveSingleton = AnalysisData::UnaryOperatorInCondition; 
                        }
                    }

                    // Context:    instance == nullptr  /  instance == NULL
                    else if (auto* bn = dyn_cast<BinaryOperator>(se)) {
                        
                        if (isEqualBetween<DeclRefExpr, CXXNullPtrLiteralExpr>(bn)) 
                            analysisData.typeNaiveSingleton = AnalysisData::BinaryOperatorInConditionNullptr; 
                        else if (isEqualBetween<DeclRefExpr, GNUNullExpr>(bn)) 
                            analysisData.typeNaiveSingleton = AnalysisData::BinaryOperatorInConditionNull; 
                        else 
                            analysisData.typeNaiveSingleton = AnalysisData::UnknownCondition; 
                       
                        if (analysisData.typeNaiveSingleton != AnalysisData::UnknownCondition)
                            se = bn->getLHS();
                    }
                    
                    if (auto* declRef = dyn_cast<DeclRefExpr>(se)) {
                        ValueDecl* vd = declRef->getDecl();
                        if (VarDecl *varDecl = dyn_cast<VarDecl>(vd)) {
                            if (varDecl->isStaticLocal() 
                            || (varDecl->isStaticDataMember() && (varDecl->getAccess() == AS_private)))
                                analysisData.instanceField = varDecl;
                        }
                    }
                }
           }
           return false;
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
        unsigned int amountObjects          : 27;
        
        enum TypeOfNaiveSingleton {
            UnaryOperatorInCondition,
            BinaryOperatorInConditionNullptr,
            BinaryOperatorInConditionNull,
            UnknownCondition,
        } typeNaiveSingleton;

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
            printField("Multiple instances detected", notSingleton ? " ✗ YES" : "✓ NO");
            
            // Final conclusion
            llvm::outs() << "╠══════════════════════════════════════════════════════════════════╣\n";
            
            
            std::string conclusion = "Conclusion: " + std::string(notSingleton ? " ✓ LIKELY SINGLETON" : "✗ NOT A SINGLETON");
            printLine(conclusion);
            llvm::outs() << "╚══════════════════════════════════════════════════════════════════╝\n";
            llvm::outs() << "\n";
            
        }

    } analysisData;
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
