#!/usr/bin/python3

# clang --include-directory /home/nubots/build/shared --include-directory /home/nubots/NUbots/shared --include-directory /home/nubots/NUbots/nuclear/message/include -c -Xclang -ast-dump -fsyntax-only test.cpp
# pydoc -b tools/clang/cindex.py

import sys
import os

import b
from dockerise import run_on_docker
import analyse


def generateReactorMarkdown(reactor):
    out = "## {}".format(reactor.getType())
    out += "\n{}".format(reactor.getBrief())
    for _, method in reactor.getMethodsNoDuplicate().items():
        out += "\n### {}".format(method.getName())
        out += "\n{}".format(method.getBrief())
        for on in method.getOns():
            out += "\n* {}".format(str(on).replace("<", "\<"))
            out += " {}".format(on.getBrief())
        for emit in method.getEmits():
            out += "\n* {}".format(str(emit).replace("<", "\<"))
            out += " {}".format(emit.getBrief())
    out += "\n"
    return out


def generateModuleMarkdown(module, reactors):
    out = "# " + "::".join(module.split("/"))
    for reactor in reactors:
        out += "\n" + generateReactorMarkdown(reactor)
    out += "\n"
    return out


@run_on_docker
def register(command):
    command.help = "documentation generation?"

    command.add_argument("outdir", help="The output directory")

    command.add_argument("--indir", default="module", help="The root of the directories that you want to scan")


@run_on_docker
def run(outdir, indir, **kwargs):
    index = analyse.createIndex()

    # analyse.printTree(analyse.translate(index, os.path.join(indir)).translationUnit.cursor)
    # return

    modules = {}

    for dirpath, dirnames, filenames in os.walk(indir):
        if dirpath.split("/")[-1] == "src":
            modules["/".join(dirpath.split("/")[0:-1])] = filenames

    for module, files in modules.items():
        print("Working on module", module)
        reactors = []
        for f in files:
            if os.path.splitext(f)[1] == ".cpp":
                print("    Working on file", f)
                translationUnit = analyse.translate(index, os.path.join(module, "src", f))

                # Check that the parsing of the tu succeeded
                failed = False
                for diagnostic in translationUnit.getDiagnostics():
                    # If the diagnostic is an error or fatal, print and don't look at the errornous translation unit
                    if diagnostic.category_number > 2:
                        print(diagnostic, ", ", module, "/src/", f, " will not be looked at", sep="")
                        failed = True
                if failed:
                    continue

                reactors += translationUnit.getReactors()

        toWrite = open(os.path.join(outdir, "_".join(module.split("/")) + ".md",), "w")
        toWrite.write(generateModuleMarkdown(module, reactors))
        toWrite.close()
