// Exports Ghidra's view of the program for cross-checking against tools/funcs.py and tools/xbe.py.
// Output is derived from the game binary: write it only under analysis-local/ (git-ignored).
//   analyzeHeadless ... -scriptPath tools/ghidra -postScript ExportInventory.java <output directory>
//@category KND
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;

import java.io.File;
import java.io.PrintWriter;

public class ExportInventory extends GhidraScript {
    @Override
    public void run() throws Exception {
        File out = new File(getScriptArgs()[0]);
        out.mkdirs();
        try (PrintWriter w = new PrintWriter(new File(out, "blocks.tsv"), "UTF-8")) {
            w.println("name\tstart\tsize\tinitialized\tread\twrite\texecute");
            for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
                w.printf("%s\t%s\t%d\t%b\t%b\t%b\t%b%n", b.getName(), b.getStart(), b.getSize(), b.isInitialized(),
                         b.isRead(), b.isWrite(), b.isExecute());
            }
        }
        try (PrintWriter w = new PrintWriter(new File(out, "functions.tsv"), "UTF-8")) {
            w.println("entry\tmin\tmax\tbytes\tranges\tthunk\tsource\tname");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                monitor.checkCancelled();
                AddressSetView body = f.getBody();
                w.printf("%s\t%s\t%s\t%d\t%d\t%b\t%s\t%s%n", f.getEntryPoint(), body.getMinAddress(),
                         body.getMaxAddress(), body.getNumAddresses(), body.getNumAddressRanges(), f.isThunk(),
                         f.getSymbol().getSource(), f.getName(true));
            }
        }
        try (PrintWriter w = new PrintWriter(new File(out, "entrypoints.tsv"), "UTF-8")) {
            w.println("address\tname");
            for (Address a : currentProgram.getSymbolTable().getExternalEntryPointIterator()) {
                Symbol s = currentProgram.getSymbolTable().getPrimarySymbol(a);
                w.printf("%s\t%s%n", a, s == null ? "" : s.getName());
            }
        }
        try (PrintWriter w = new PrintWriter(new File(out, "symbols.tsv"), "UTF-8")) {
            w.println("address\tsource\ttype\tname");
            for (Symbol s : currentProgram.getSymbolTable().getAllSymbols(true)) {
                monitor.checkCancelled();
                if (s.getSource() == ghidra.program.model.symbol.SourceType.DEFAULT) {
                    continue;
                }
                w.printf("%s\t%s\t%s\t%s%n", s.getAddress(), s.getSource(), s.getSymbolType(), s.getName(true));
            }
        }
        long instructions = 0;
        try (PrintWriter w = new PrintWriter(new File(out, "code_ranges.tsv"), "UTF-8")) {
            w.println("start\tend\tinstructions");
            Address start = null, end = null;
            long n = 0;
            for (Instruction i : currentProgram.getListing().getInstructions(true)) {
                monitor.checkCancelled();
                if (start == null || !i.getAddress().equals(end.next())) {
                    if (start != null) {
                        w.printf("%s\t%s\t%d%n", start, end, n);
                    }
                    start = i.getAddress();
                    n = 0;
                }
                end = i.getMaxAddress();
                n++;
                instructions++;
            }
            if (start != null) {
                w.printf("%s\t%s\t%d%n", start, end, n);
            }
        }
        println("ExportInventory: program " + currentProgram.getName() + ", compiler spec "
                + currentProgram.getCompilerSpec().getCompilerSpecID() + ", language "
                + currentProgram.getLanguageID() + ", image base " + currentProgram.getImageBase()
                + ", functions " + currentProgram.getFunctionManager().getFunctionCount()
                + ", instructions " + instructions);
    }
}
