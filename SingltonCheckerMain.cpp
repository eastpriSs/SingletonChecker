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
        
        auto returnType = func->getReturnType()->getPointeeType();
        
        for (Stmt* st : func->getBody()->children()){
            if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                    for (Decl *decl : declStmt->decls()) {
                        if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
                            if (varDecl->isStaticLocal() ) {
 //                               llvm::outs() << "Found local variable ";
                            }
                        }
                    }
             }
             if (auto *retStmt = dyn_cast<ReturnStmt>(st)){
                    Expr* retExpr = retStmt->getRetValue();
                    if (auto *declRef = dyn_cast<DeclRefExpr>(retExpr)){
                        ValueDecl *valueDecl = declRef->getDecl();
                        if (VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl)) {
                            if (varDecl->isStaticLocal() && varDecl->getType()->getCanonicalTypeUnqualified() == returnType)
                                llvm::outs() << "";
                        }
                    }          
              }
        }
        return true;
    }
};


class ClassVisitor : public RecursiveASTVisitor<ClassVisitor> {
private:

        bool isClassObject(VarDecl* varDecl, CXXRecordDecl* clssDecl)
        {
            if (!(varDecl && clssDecl)) return false;
            return !varDecl->getType()->isPointerType()
                && (varDecl->getType()->getCanonicalTypeUnqualified() == clssDecl->getTypeForDecl()->getCanonicalTypeUnqualified());
        }
       
        void checkForSingleObjectOfParentClass(CXXMethodDecl* method)
        {
            if (!method->hasBody()) return;
           
            for (Stmt* st : method->getBody()->children()){
                if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                    for (Decl* dcl : declStmt->decls()) {
                        if (VarDecl* varDecl = dyn_cast<VarDecl>(dcl)){
                            if (isClassObject(varDecl, method->getParent())) {
                                //
                                //                                         | if found temporary object
                                //                                         V 
                                if (++classStatistics.amountObjects > 1 || !varDecl->isStaticLocal()) {
                                    classStatistics.notSingleton = true;
                                    return;
                                }
                            }
                        }
                    }
               }
            } 
        }

        bool isProbablyGetInstanceMethod(CXXMethodDecl *method) 
        {  
           if (!method->hasBody()) return false;
           auto returnType = method->getReturnType()->getPointeeType();

           for (Stmt* st : method->getBody()->children()) {
                if (auto *retStmt = dyn_cast<ReturnStmt>(st)){
                    Expr* retExpr = retStmt->getRetValue();
                    if (auto *declRef = dyn_cast<DeclRefExpr>(retExpr)){
                        ValueDecl *valueDecl = declRef->getDecl();
                        if (VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl)) {
                            return (varDecl->isStaticLocal() 
                                    && (varDecl->getType()->getCanonicalTypeUnqualified() == returnType)) 
                            || (varDecl->isStaticDataMember()
                               && (varDecl->getAccess() == AS_private)
                               && (varDecl->getType()->getCanonicalTypeUnqualified() == returnType));
                        }
                    }
                }
           }
           return false;
        }
public:
    explicit ClassVisitor(ASTContext *Context) : Context(Context) {}

    bool VisitCXXRecordDecl(CXXRecordDecl *declaration) {
        if (declaration->isEmbeddedInDeclarator() && !declaration->isFreeStanding()) {
            return true;
        }
        
        classStatistics.clear();
        
        // first stage of analysis
        for (const auto* c : declaration->ctors())
            if (c->getAccess() == AS_public && !c->isDeleted())
               classStatistics.ctorsPrivate = false; 
        
        for (auto *method : declaration->methods()) {
            if (method->isStatic() 
            && (method->getAccess() == AS_public)
            && (method->getReturnType()->getPointeeType() == declaration->getTypeForDecl()->getCanonicalTypeUnqualified())){
                classStatistics.hasMethodLikelyInstance = isProbablyGetInstanceMethod(method); 
            }
                
            if (CXXConstructorDecl* ctrDecl = dyn_cast<CXXConstructorDecl>(method))
                if (ctrDecl->isCopyConstructor() && ctrDecl->isDeleted())
                    classStatistics.hasDeletedCopyConstuctor = true;
                    
            if (method->isCopyAssignmentOperator() && method->isDeleted())
                classStatistics.hasDeletedAssigmentOperator = true;
        
            // second stage of analysis
            checkForSingleObjectOfParentClass(method);
        }

        // second stage of analysis  
        if (!classStatistics.notSingleton) {
            for (auto* field : declaration->decls()) {
                if (isClassObject(dyn_cast<VarDecl>(field), declaration)) { 
                    if (++classStatistics.amountObjects > 1) {
                        classStatistics.notSingleton = true;
                    }
                }
                if (classStatistics.notSingleton) break;
            }
        }

        classStatistics.dump();
        return true;
    }

private:
    ASTContext *Context;

    struct SingletonSigns { // TODO rename
        bool ctorsPrivate                   : 1; 
        bool hasMethodLikelyInstance        : 1; 
        bool hasDeletedCopyConstuctor       : 1; 
        bool hasDeletedAssigmentOperator    : 1;
        bool notSingleton                   : 1;
        unsigned int amountObjects          : 4;
        inline void  clear() noexcept
        {
            ctorsPrivate = true;
            hasMethodLikelyInstance = false;
            hasDeletedCopyConstuctor = false;
            hasDeletedAssigmentOperator = false;
            notSingleton = false;
            amountObjects = 0; 
        }
        inline void dump() const noexcept  
        {
            llvm::outs() << ctorsPrivate 
                         << hasMethodLikelyInstance
                         << hasDeletedCopyConstuctor
                         << hasDeletedAssigmentOperator
                         << notSingleton
                         << amountObjects;
        }

    } classStatistics;
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
