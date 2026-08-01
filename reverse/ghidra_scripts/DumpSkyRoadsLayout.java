// Dump the address model selected by Ghidra's DOS MZ loader.
// @category SkyRoads

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.mem.MemoryBlock;

public class DumpSkyRoadsLayout extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: DumpSkyRoadsLayout.java <output-file>");
        }

        File output = new File(args[0]);
        File parent = output.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }

        try (PrintWriter writer = new PrintWriter(output, "UTF-8")) {
            writer.println("program=" + currentProgram.getName());
            writer.println("language=" + currentProgram.getLanguageID());
            writer.println("compiler=" + currentProgram.getCompilerSpec().getCompilerSpecID());
            writer.println("image_base=" + currentProgram.getImageBase());
            writer.println("min_address=" + currentProgram.getMinAddress());
            writer.println("max_address=" + currentProgram.getMaxAddress());

            writer.println("[address_spaces]");
            for (AddressSpace space : currentProgram.getAddressFactory().getAllAddressSpaces()) {
                writer.printf("%s type=%d bits=%d%n", space.getName(), space.getType(), space.getSize());
            }

            writer.println("[memory_blocks]");
            for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
                writer.printf("%s %s %s initialized=%s execute=%s%n",
                    block.getName(), block.getStart(), block.getEnd(),
                    block.isInitialized(), block.isExecute());
            }

            writer.println("[functions]");
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            for (Function function : functions) {
                writer.printf("%s %s%n", function.getEntryPoint(), function.getName());
            }
        }
    }
}
