#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>
#include <string>

using namespace clang;

namespace {

class ClassVisitor : public RecursiveASTVisitor<ClassVisitor> {
private:
        void checkGetInstatnceMethod(CXXMethodDecl *method) 
        {   
           std::vector<std::string> probablySingletonVars;
           probablySingletonVars.reserve(20);

           auto returnType = method->getReturnType()->getPointeeType();
            
           for (Stmt* st : method->getBody()->children()) {
                if (auto *declStmt = dyn_cast<DeclStmt>(st)) {
                    for (Decl *decl : declStmt->decls()) {
                        if (auto *varDecl = dyn_cast<VarDecl>(decl)) {
                            if (varDecl->isStaticLocal()) {
                                if (varDecl->getType()->getCanonicalTypeUnqualified() == returnType)
                                    probablySingletonVars.push_back(varDecl->getName().str());
                            }
                        }
                    }
                }
                if (auto *retStmt = dyn_cast<ReturnStmt>(st)){
                    Expr* retExpr = retStmt->getRetValue();
                    if (auto *declRef = dyn_cast<DeclRefExpr>(retExpr)){
                        ValueDecl *valueDecl = declRef->getDecl();
                        if (VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl))
                            if (std::find(probablySingletonVars.begin(), probablySingletonVars.end(), varDecl->getName().str())
                                    != probablySingletonVars.end())
                                llvm::outs() << "this is singleton";
                    }
                }
            }
        }
public:
    explicit ClassVisitor(ASTContext *Context) : Context(Context) {}

    bool VisitCXXRecordDecl(CXXRecordDecl *declaration) {
        declaration->dumpColor();
        if (declaration->isEmbeddedInDeclarator() && !declaration->isFreeStanding()) {
            return true;
        }

        for (const auto* c : declaration->ctors())
            if (c->getAccess() == AS_private)
                llvm::outs() << "\n!-! Private constructor\n";

        for (auto *method : declaration->methods()) {
            if (method->isStatic() && (method->getAccess() == AS_public))
                if (method->getReturnType()->getPointeeType() == declaration->getTypeForDecl()->getCanonicalTypeUnqualified()){
                    llvm::outs() << "\n!-! Probably getInstance() function \n";
                    checkGetInstatnceMethod(method);  
                }
        }

        return true;
    }

private:
    ASTContext *Context;
};

class ClassVisitorASTConsumer : public ASTConsumer {
public:
    explicit ClassVisitorASTConsumer(ASTContext *Context) : Visitor(Context) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

private:
    ClassVisitor Visitor;
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
