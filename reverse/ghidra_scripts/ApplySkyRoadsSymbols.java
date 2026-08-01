// Create reviewed function entries and apply semantic names from symbols.tsv.
// @category SkyRoads

import java.io.BufferedReader;
import java.io.FileReader;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class ApplySkyRoadsSymbols extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("usage: ApplySkyRoadsSymbols.java <symbols.tsv>");
        }

        int applied = 0;
        try (BufferedReader reader = new BufferedReader(new FileReader(args[0]))) {
            String line = reader.readLine(); // header
            while ((line = reader.readLine()) != null) {
                String[] fields = line.split("\\t", 4);
                if (fields.length != 4) {
                    printerr("Skipping malformed symbol row: " + line);
                    continue;
                }

                int offset = Integer.decode(fields[0]);
                Address address = currentProgram.getAddressFactory().getAddress(
                    String.format("1000:%04x", offset));
                if (address == null) {
                    printerr("No MZ address for " + fields[0]);
                    continue;
                }

                Function function = getFunctionAt(address);
                if (function == null) {
                    disassemble(address);
                    function = createFunction(address, null);
                }
                if (function == null) {
                    printerr("Could not create function at " + address + " for " + fields[1]);
                    continue;
                }

                function.setName(fields[1], SourceType.USER_DEFINED);
                currentProgram.getListing().setComment(address, CodeUnit.PLATE_COMMENT,
                    fields[2] + ": " + fields[3]);
                applied++;
            }
        }
        println("Applied " + applied + " reviewed SkyRoads symbols");
    }
}
