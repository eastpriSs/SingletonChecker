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
                                //llvm::outs() << "type.Singleton function sign ||  ";
                        }
                    }          
              }
        }
        return true;
    }
};


class ClassVisitor : public RecursiveASTVisitor<ClassVisitor> {
private:
        void checkGetInstatnceMethod(CXXMethodDecl *method) 
        {   
           auto returnType = method->getReturnType()->getPointeeType();

           for (Stmt* st : method->getBody()->children()) {
                if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                    for (Decl *decl : declStmt->decls()) {
                        if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
                            if (varDecl->isStaticLocal()) {
                                if (varDecl->getType()->getCanonicalTypeUnqualified() == returnType) {
                                    llvm::outs() << "Found static local variable with class parent type. Singleton sign ||";
                                }
                            }
                        }
                    }
                }
                if (auto *retStmt = dyn_cast<ReturnStmt>(st)){
                    Expr* retExpr = retStmt->getRetValue();
                    if (auto *declRef = dyn_cast<DeclRefExpr>(retExpr)){
                        ValueDecl *valueDecl = declRef->getDecl();
                        if (VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl)) {
                           if (varDecl->isStaticLocal() && (varDecl->getType()->getCanonicalTypeUnqualified() == returnType))
                              llvm::outs() 
                              << "Found return statement with static local variable with parent type.Singleton sign ||  ";
                           else if (varDecl->isStaticDataMember()
                                    && (varDecl->getAccess() == AS_private)
                                    && (varDecl->getType()->getCanonicalTypeUnqualified() == returnType)) 
                              llvm::outs()
                              << "Found return statement with private static field of class with parent type.Singleton sign ||  ";
                            
                        }
                    }
                }
            }
        }
public:
    explicit ClassVisitor(ASTContext *Context) : Context(Context) {}

    bool VisitCXXRecordDecl(CXXRecordDecl *declaration) {
//        declaration->dumpColor();
        if (declaration->isEmbeddedInDeclarator() && !declaration->isFreeStanding()) {
            return true;
        }

        for (const auto* c : declaration->ctors())
            if (c->getAccess() == AS_private)
                llvm::outs() << "\n Found private constructor. Singleton sign ||\n";

        for (auto *method : declaration->methods()) {
            if (method->isStatic() && (method->getAccess() == AS_public))
                if (method->getReturnType()->isPointerType() &&
                   (method->getReturnType()->getPointeeType() == declaration->getTypeForDecl()->getCanonicalTypeUnqualified())){
                    llvm::outs() << "\n!-! Probably getInstance() function \n";
                    checkGetInstatnceMethod(method);  
                }
                
                if (CXXConstructorDecl* ctrDecl = dyn_cast<CXXConstructorDecl>(method))
                    if (ctrDecl->isCopyConstructor() && ctrDecl->isDeleted())
                        llvm::outs() << "\nCopy constructor deleted !\n";
                
                if (method->isCopyAssignmentOperator() && method->isDeleted())
                    llvm::outs() << "\nCopy assigment operator  deleted !\n";
        }

        return true;
    }

private:
    ASTContext *Context;
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
