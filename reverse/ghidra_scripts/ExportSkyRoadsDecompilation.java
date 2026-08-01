// Export deterministic C-like decompiler output for every discovered function.
// @category SkyRoads

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ExportSkyRoadsDecompilation extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException(
                "usage: ExportSkyRoadsDecompilation.java <output-file>");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.setOptions(new DecompileOptions());
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("Decompiler initialization failed: " +
                decompiler.getLastMessage());
        }

        int exported = 0;
        try (PrintWriter writer = new PrintWriter(output, "UTF-8")) {
            writer.println("/* Generated from skyroads.exe by Ghidra; do not edit. */");
            writer.println("/* Reviewed source belongs in reverse/reconstructed/. */");
            writer.println();

            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                DecompileResults result = decompiler.decompileFunction(function, 30, monitor);
                writer.printf("/* %s %s */%n", function.getEntryPoint(), function.getName());
                if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                    writer.println(result.getDecompiledFunction().getC());
                }
                else {
                    writer.println("/* DECOMPILATION FAILED: " + result.getErrorMessage() + " */");
                }
                writer.println();
                exported++;
            }
        }
        finally {
            decompiler.dispose();
        }
        println("Exported " + exported + " functions to " + output);
    }
}
