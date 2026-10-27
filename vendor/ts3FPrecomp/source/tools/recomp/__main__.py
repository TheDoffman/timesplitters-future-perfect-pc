"""
Xbox x86 → C Static Recompiler

Usage:
    py -3 -m tools.recomp <xbe_path> [options]

Options:
    -o, --output-dir DIR    Output directory (default: tools/recomp/output)
    --game-name NAME        Game name stamped into generated-code banners
                            (default: "Xbox Game")
    -f, --function ADDR     Translate a single function (hex address)
    -c, --category CAT      Translate functions of a specific category
    --game-only             Only translate game_engine + game_vtable + unknown functions
    --all                   Translate all functions (including RW, CRT, XDK)
    -n, --max-funcs N       Maximum number of functions to translate
    -v, --verbose           Verbose output
    --list-categories       List available function categories and counts
    --header                Generate C header with forward declarations
"""

import argparse
import json
import os
import sys
import time

from . import config
from .translator import BatchTranslator
from .output import write_summary, print_stats, generate_header


def find_data_files(disasm_dir=None, func_id_dir=None, abi_dir=None, overrides=None):
    """Locate the disasm/func_id output files.

    Defaults to the in-tree tools/*/output directories. A game project living
    outside this repo passes --disasm-dir/--func-id-dir (or the per-file
    overrides) to point at its own pipeline output instead.
    """
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    disasm_dir = disasm_dir or os.path.join(base, "disasm", "output")
    func_id_dir = func_id_dir or os.path.join(base, "func_id", "output")
    abi_dir = abi_dir or os.path.join(base, "abi_analysis", "output")

    paths = {
        "functions": os.path.join(disasm_dir, "functions.json"),
        "labels": os.path.join(disasm_dir, "labels.json"),
        "identified": os.path.join(func_id_dir, "identified_functions.json"),
        "abi": os.path.join(abi_dir, "abi_functions.json"),
        "summary": os.path.join(disasm_dir, "summary.json"),
    }
    paths.update({k: v for k, v in (overrides or {}).items() if v})

    for key, path in paths.items():
        if not os.path.exists(path):
            if key != "summary":
                print(f"WARNING: {key} not found at {path}", file=sys.stderr)
            paths[key] = None

    return paths


def _parse_force_returns(items):
    """Parse --force-return ADDR=VALUE pairs into {addr: value}."""
    out = {}
    for item in items or ():
        if "=" not in item:
            raise SystemExit(f"--force-return wants ADDR=VALUE, got {item!r}")
        addr, _, value = item.partition("=")
        out[int(addr, 0)] = int(value, 0) & 0xFFFFFFFF  # -1 is 0xFFFFFFFF
    return out


def _load_addrs(path):
    """Load a JSON address list (or {addr: name} map) as a set of ints."""
    if not path:
        return set()
    with open(path, "r", encoding="utf-8") as fh:
        entries = json.load(fh)
    if isinstance(entries, dict):
        entries = list(entries.keys())
    out = set()
    for e in entries:
        if isinstance(e, dict):
            e = e.get("start") or e.get("address")
        out.add(int(e, 16) if isinstance(e, str) else int(e))
    return out


def check_data_matches_binary(xbe_path, summary_path):
    """Refuse to lift one binary's code using another binary's disassembly.

    The input paths default to shared in-tree directories, so a stale run from a
    different game silently produces a full set of plausible-looking C that
    belongs to the wrong binary. The disassembler records which file it read;
    compare it and stop if it disagrees.
    """
    if not summary_path:
        return

    try:
        with open(summary_path, "r", encoding="utf-8") as fh:
            recorded = json.load(fh).get("binary")
    except (OSError, ValueError):
        return

    if not recorded:
        return

    if os.path.basename(recorded).lower() != os.path.basename(xbe_path).lower():
        print(
            f"ERROR: disassembly is for '{os.path.basename(recorded)}' but you "
            f"asked to recompile '{os.path.basename(xbe_path)}'.\n"
            f"       Re-run tools.disasm on this binary, or point --disasm-dir "
            f"at the matching output.",
            file=sys.stderr,
        )
        sys.exit(1)


def game_categories(classification_db):
    """Every category that is game code, plus the unclassified remainder.

    Naming the three categories game_engine/game_vtable/unknown explicitly
    silently dropped every subsystem func_id manages to identify: on Burnout 2
    that excluded game_render, game_audio, game_network, game_vehicle, game_io,
    game_input, game_ui and game_physics. The symptom is an unresolved indirect
    call into code that was discovered and classified but never translated --
    the sharper the classifier gets, the more --game-only leaves out.

    Take every "game_*" category instead, so a new one is included by default,
    and keep "unknown" because an unclassified function is far more likely to
    be game code than library code.

    "crt" is included too, because nothing replaces it. The CRT is statically
    linked into the XBE and the runtime provides no native substitute, so an
    excluded CRT function does not become a fast native call -- it becomes a
    stub that pops the return address and returns whatever was already in eax.
    Burnout 2 calls _ftol2 510 times; stubbing it makes every float-to-int
    conversion in the game produce garbage, which surfaces much later as a
    corrupt pointer in an unrelated function. Excluding library code is only
    sound once something actually implements it.

    "data_init" is included for exactly the same reason, and it took Black to
    notice. func_id's stub_pattern classifier gives that category to any short
    function that is a chain of SSE float moves ending in ret, on the theory
    that a static initialiser has already done its work by the time the image
    is on disc. They are ordinary functions, and Black calls sixteen of them
    through function pointers: every one came back "unresolved call target",
    every one returned 0, and the title presented 3,774 entirely black frames.
    Seeding them did not help and could not have -- they were found, they were
    classified, they became function starts, and then this set dropped them
    before anything was translated. That loop is worth naming, because from
    the outside it looks exactly like a seed file that is not being read:
    tools.seed_from_log reports them as "already seeded" on every subsequent
    run while the runtime goes on reporting them as unresolved.

    The paragraph above said the sharper the classifier gets, the more
    --game-only leaves out, and then this happened one category later. So the
    rule to apply to a new category is the one that was already right for crt:
    a category may only be excluded once something actually implements it, and
    a classifier deciding code is uninteresting is not an implementation.
    """
    categories = {"unknown", "crt", "data_init"}
    for entry in classification_db.values():
        category = entry.get("category")
        if category and category.startswith("game_"):
            categories.add(category)
    return categories


def list_categories(translator):
    """Print category breakdown."""
    cats = {}
    for addr, func_info in sorted(translator.func_db.items()):
        cls = translator.classification_db.get(addr, {})
        cat = cls.get("category", "unknown")
        cats[cat] = cats.get(cat, 0) + 1

    print(f"\nFunction categories ({len(translator.func_db)} total):")
    print(f"{'Category':<30} {'Count':>8} {'Pct':>8}")
    print("-" * 48)
    for cat, count in sorted(cats.items(), key=lambda x: -x[1]):
        pct = count / len(translator.func_db) * 100
        print(f"{cat:<30} {count:>8} {pct:>7.1f}%")


def _report_unimplemented(stats):
    """List the mnemonics that were emitted as TODO comments.

    An unimplemented instruction is not a build error -- it is a comment, and
    the generated function keeps running with whatever the destination already
    held. That is the worst kind of failure: it looks like it worked. Wreckless
    booted for weeks with `bsf eax, ecx` translated to nothing, which made the
    CRT heap's free-list bitmap scan return its own argument and hand out the
    address of an empty list head.

    Sorted by count, with one example address each so the caller can go and
    look at it.
    """
    unimplemented = stats.get("unimplemented") or {}
    if not unimplemented:
        return
    total = sum(len(a) for a in unimplemented.values())
    print(f"{total} instruction(s) unimplemented, emitted as no-op comments "
          f"({len(unimplemented)} distinct mnemonics):", file=sys.stderr)
    ranked = sorted(unimplemented.items(),
                    key=lambda kv: (-len(kv[1]), kv[0]))
    for mnemonic, addrs in ranked[:20]:
        print(f"    {len(addrs):6d}  {mnemonic:<12} e.g. 0x{min(addrs):08X}",
              file=sys.stderr)
    if len(ranked) > 20:
        print(f"    ... and {len(ranked) - 20} more", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description="Xbox x86 -> C Static Recompiler")
    parser.add_argument("xbe_path", help="Path to default.xbe")
    parser.add_argument("--game-name",
                        help="Game name stamped into generated-code banners")
    parser.add_argument("-o", "--output-dir",
                        help="Output directory")
    parser.add_argument("-f", "--function",
                        help="Translate single function (hex address)")
    parser.add_argument("-c", "--category",
                        help="Translate functions of a category")
    parser.add_argument("--game-only", action="store_true",
                        help="Only game functions (game_engine, game_vtable, unknown)")
    parser.add_argument("--all", action="store_true",
                        help="Translate all functions")
    parser.add_argument("-n", "--max-funcs", type=int,
                        help="Max functions to translate")
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("--list-categories", action="store_true",
                        help="List categories and exit")
    parser.add_argument("--header", action="store_true",
                        help="Generate C header file")
    parser.add_argument("--split", type=int, metavar="N",
                        help="Split output into files of N functions each")
    parser.add_argument("--gen-dir",
                        help="Output dir for split generated files "
                             "(default: src/game/recomp/gen)")
    parser.add_argument("--disasm-dir",
                        help="Directory holding tools.disasm output "
                             "(default: tools/disasm/output)")
    parser.add_argument("--func-id-dir",
                        help="Directory holding tools.func_id output "
                             "(default: tools/func_id/output)")
    parser.add_argument("--abi-dir",
                        help="Directory holding tools.abi_analysis output "
                             "(default: tools/abi_analysis/output)")
    parser.add_argument("--functions",
                        help="Path to functions.json (overrides --disasm-dir)")
    parser.add_argument("--labels",
                        help="Path to labels.json (overrides --disasm-dir)")
    parser.add_argument("--identified",
                        help="Path to identified_functions.json "
                             "(overrides --func-id-dir)")
    parser.add_argument("--abi",
                        help="Path to abi_functions.json (overrides --abi-dir)")
    parser.add_argument("--skip-binary-check", action="store_true",
                        help="Allow disassembly recorded for a different binary")
    parser.add_argument("--icall-sites", metavar="FILE", default=None,
                        help="Per-site indirect-call targets from "
                             "tools.recomp.icall_feedback merge --sites-db "
                             "(default: icall_sites.json in the -o directory; "
                             "scripts/recompile.py passes the --work-dir one)")
    parser.add_argument("--manual-functions", metavar="FILE",
                        help="JSON list of addresses the project implements by "
                             "hand. Their bodies are not generated, so the "
                             "hand-written definition links instead")
    parser.add_argument("--hle-symbols", metavar="FILE",
                        help="tools.xdk_symbols JSON for this XBE. XDK functions "
                             "with an HLE_EXPORT(Name) implementation are "
                             "replaced by name instead of lifted "
                             "(see tools/recomp/hle.py)")
    parser.add_argument("--hle-impl", metavar="PATH", action="append",
                        help="C file or directory to scan for HLE_EXPORT(Name) "
                             "markers (repeatable; default: src/hle)")
    parser.add_argument("--only-hle-thunks", action="store_true",
                        help="Rewrite gen/recomp_hle.c against the current "
                             "src/hle and exit, without lifting anything. "
                             "Adding an HLE_EXPORT or HLE_ORIGINAL to src/hle "
                             "leaves every already-lifted title unable to link "
                             "until its recomp_hle.c mentions the new name; "
                             "this refreshes it in seconds instead of hours. "
                             "Original bodies this lift did not emit are left "
                             "0, which the replacements report at run time.")
    parser.add_argument("--exclude-manual", metavar="FILE",
                        nargs="?", const="src/game/recomp/recomp_manual.c",
                        help="Scan a C file (default recomp_manual.c) for the "
                             "functions it defines by hand and skip generating "
                             "their bodies -- the same effect as listing them in "
                             "--manual-functions, but read straight from the "
                             "source of truth so the two cannot drift. Handles "
                             "sub_X_gen wrappers and pins referenced sub_ names. "
                             "Ported from the Burnout 3 fork.")
    parser.add_argument("--trace-functions", metavar="FILE",
                        help="JSON list of addresses to emit an entry trace "
                             "for (RECOMP_TRACE_ENTER). For bring-up: shows "
                             "which call in an init chain is not returning")
    parser.add_argument("--trace-all-entries", action="store_true",
                        help="Emit an entry hook in every function. With "
                             "RECOMP_TRACE_PROFILE set at run time this reports "
                             "how many distinct functions the run reached -- a "
                             "frontier measure that rises with progress, unlike "
                             "kernel calls (flat once startup ends) or raw "
                             "indirect-call counts (inflated by spin loops)")
    parser.add_argument("--force-return", metavar="ADDR=VALUE",
                        action="append", default=[],
                        help="Make a function hand its callers a constant "
                             "instead of what it computed, e.g. "
                             "0x0015D780=0. Repeatable. A bring-up probe for "
                             "a title waiting on a service the runtime does "
                             "not implement yet: the body still runs, only "
                             "the answer changes, and the emitted code is "
                             "inert unless RECOMP_FORCE_RETURN is set")
    parser.add_argument("--seh-prolog", metavar="ADDR",
                        help="Address of __SEH_prolog (hex). Auto-detected if omitted")
    parser.add_argument("--seh-epilog", metavar="ADDR",
                        help="Address of __SEH_epilog (hex). Auto-detected if omitted")

    args = parser.parse_args()

    if args.game_name:
        config.set_game_name(args.game_name)

    # Find data files
    data_files = find_data_files(
        disasm_dir=args.disasm_dir,
        func_id_dir=args.func_id_dir,
        abi_dir=args.abi_dir,
        overrides={
            "functions": args.functions,
            "labels": args.labels,
            "identified": args.identified,
            "abi": args.abi,
        },
    )
    if not data_files["functions"]:
        print("ERROR: functions.json not found. Run the disassembler first.",
              file=sys.stderr)
        sys.exit(1)

    if not args.skip_binary_check:
        check_data_matches_binary(args.xbe_path, data_files.get("summary"))

    # Derive the memory map from the binary we were handed. Skipping this leaves
    # the fallback layout in place, and every VA outside it lifts as "not code".
    try:
        config.configure_from_xbe(args.xbe_path)
    except Exception as e:
        print(f"ERROR: could not read section layout from {args.xbe_path}: {e}",
              file=sys.stderr)
        sys.exit(1)

    if args.verbose:
        print(f"Section layout from {os.path.basename(args.xbe_path)}: "
              f"{len(config.SECTIONS)} sections, "
              f".text 0x{config.TEXT_VA_START:08X}-0x{config.TEXT_VA_END:08X}",
              file=sys.stderr)

    print(f"Loading data files...", file=sys.stderr)
    t0 = time.time()

    translator = BatchTranslator(
        xbe_path=args.xbe_path,
        func_json_path=data_files["functions"],
        labels_json_path=data_files.get("labels"),
        identified_json_path=data_files.get("identified"),
        abi_json_path=data_files.get("abi"),
        output_dir=args.output_dir,
        trace_functions=_load_addrs(args.trace_functions),
        trace_all_entries=args.trace_all_entries,
        force_returns=_parse_force_returns(args.force_return),
        seh_prolog=int(args.seh_prolog, 16) if args.seh_prolog else None,
        seh_epilog=int(args.seh_epilog, 16) if args.seh_epilog else None,
        # Per title, never the shared tools output: two titles lifted side
        # by side would otherwise guard one game's call sites with the
        # other's targets. Without --icall-sites it follows -o.
        icall_sites_json_path=args.icall_sites or os.path.join(
            args.output_dir or os.path.join(
                os.path.dirname(os.path.abspath(__file__)), "output"),
            "icall_sites.json"),
    )

    t_load = time.time() - t0
    print(f"Loaded {len(translator.func_db)} functions, "
          f"{len(translator.label_db)} labels, "
          f"{len(translator.classification_db)} classifications, "
          f"{len(translator.abi_db)} ABI entries "
          f"in {t_load:.1f}s", file=sys.stderr)

    # List categories mode
    if args.list_categories:
        list_categories(translator)
        return

    # Single function mode
    if args.function:
        addr = int(args.function, 16)
        code = translator.translate_single(addr)
        if code:
            print(code)
        else:
            print(f"ERROR: Could not translate function at 0x{addr:08X}",
                  file=sys.stderr)
            sys.exit(1)
        return

    # Generate header mode
    if args.header:
        output_dir = args.output_dir or os.path.join(
            os.path.dirname(__file__), "output")
        os.makedirs(output_dir, exist_ok=True)

        if args.game_only:
            funcs = translator.get_functions_by_category(
                categories=game_categories(translator.classification_db))
        elif args.category:
            funcs = translator.get_functions_by_category(
                categories={args.category})
        else:
            funcs = translator.get_functions_by_category()

        header_path = os.path.join(output_dir, "recomp_functions.h")
        generate_header(funcs, header_path, abi_db=translator.abi_db,
                        title=translator.title)
        print(f"Generated header: {header_path} ({len(funcs)} declarations)")
        return

    # Batch translation
    t0 = time.time()

    if args.category:
        categories = {args.category}
        funcs = translator.get_functions_by_category(categories=categories)
    elif args.game_only:
        # Game-specific functions only
        categories = game_categories(translator.classification_db)
        funcs = translator.get_functions_by_category(categories=categories)
    elif args.all:
        funcs = translator.get_functions_by_category()
    else:
        # Default: game functions only
        categories = game_categories(translator.classification_db)
        funcs = translator.get_functions_by_category(categories=categories)

    print(f"\nTranslating {len(funcs)} functions...", file=sys.stderr)

    if args.split:
        # Split output mode: multiple .c files + header + dispatch table
        gen_dir = args.gen_dir or os.path.join(
            os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
            "src", "game", "recomp", "gen")

        if args.max_funcs:
            funcs = funcs[:args.max_funcs]

        manual = set()
        if args.manual_functions:
            with open(args.manual_functions, "r", encoding="utf-8") as fh:
                entries = json.load(fh)
            # Accept a bare list of addresses or the {addr: name} shape the
            # naming tools emit, so a project can point this at either.
            #
            # With {addr: name}, the name is binding: the hand-written C
            # defines that symbol, so the generated declaration and every call
            # site must use it regardless of what the function database calls
            # the address. Otherwise re-running the naming tools silently
            # renames the address, the manual definition no longer matches, and
            # the only symptom is a pile of unresolved externals at link time
            # that say nothing about naming.
            pinned = entries if isinstance(entries, dict) else {}
            if isinstance(entries, dict):
                entries = list(entries.keys())
            for e in entries:
                if isinstance(e, dict):
                    e = e.get("start") or e.get("address")
                manual.add(int(e, 16) if isinstance(e, str) else int(e))
            for key, pinned_name in pinned.items():
                if not pinned_name:
                    continue
                addr = int(key, 16) if isinstance(key, str) else int(key)
                entry = translator.func_db.get(addr)
                if entry is None:
                    print(f"  warning: manual function 0x{addr:08X} is not in "
                          f"the function database", file=sys.stderr)
                    continue
                if entry.get("name") != pinned_name:
                    print(f"  pinned 0x{addr:08X}: "
                          f"{entry.get('name')} -> {pinned_name}",
                          file=sys.stderr)
                entry["name"] = pinned_name
            print(f"Hand-written overrides: {len(manual)} functions will not "
                  f"be generated", file=sys.stderr)

        # --exclude-manual: derive the same `manual` set by scanning the C file
        # the project hand-writes, instead of a separate JSON that has to be
        # kept in sync with it. Everything below maps onto mechanisms already
        # used above -- the `manual` set (declare-only) and func_db name pinning
        # -- so the translator needs no changes.
        if args.exclude_manual:
            from .manual_scan import scan as _scan_manual
            skip, wrap, referenced = _scan_manual(args.exclude_manual)
            known = set(translator.func_db)

            # referenced-but-not-wrapped: the hand-written code names these as
            # sub_XXXXXXXX (declares or calls them), so they must keep that
            # name whatever a naming pass wanted. Otherwise the manual reference
            # is left undefined at link.
            pinned = 0
            for addr in (referenced & known) - wrap:
                info = translator.func_db[addr]
                plain = f"sub_{addr:08X}"
                if info.get("name") != plain:
                    info["name"] = plain
                    pinned += 1

            # wrap: recomp_manual.c defines sub_X itself and calls the generated
            # body as sub_X_gen. So do NOT add these to `manual` (the body is
            # still needed) -- just rename them so the emitted body is sub_X_gen.
            #
            # Only the body. Every call to the function, and its dispatch-table
            # entry, must still name sub_X -- the wrapper -- or the wrapper is
            # defined and never reached. Renaming "name" alone did exactly that:
            # the first title to wrap anything (Marvel vs Capcom 2, 29 Sep
            # 2026) had its generated callers call sub_X_gen directly.
            for addr in wrap & known:
                translator.func_db[addr]["name"] = f"sub_{addr:08X}_gen"
                translator.func_db[addr]["call_name"] = f"sub_{addr:08X}"

            # skip - wrap: defined by hand and not wrapped -> declare-only, which
            # is exactly what membership in `manual` produces.
            newly = {a for a in (skip - wrap) if a in known} - manual
            manual |= newly
            print(f"Excluding {len(newly)} functions defined in "
                  f"{args.exclude_manual}"
                  + (f"; {pinned} pinned to sub_ names" if pinned else "")
                  + (f"; {len(wrap & known)} wrapped as sub_X_gen" if wrap else ""),
                  file=sys.stderr)

        # XDK functions replaced by name (tools/recomp/hle.py). Planned after
        # the hand-written set is complete, so title code keeps winning, and
        # before translation, so the replaced bodies are not lifted.
        hle_replace = {}
        # XDK variables the replacements import by name. Scanned whether or not
        # there is a symbols file: the implementations declare them either
        # way, so the generated file has to define them either way (as 0 when
        # they cannot be named) or the build does not link.
        from .hle import (imported_variables, load_variables, load_symbols,
                          resolve_variables)
        hle_impl_paths = args.hle_impl or [os.path.join(
            os.path.dirname(os.path.dirname(os.path.dirname(
                os.path.abspath(__file__)))), "src", "hle")]
        hle_var_names = imported_variables(
            [p for p in hle_impl_paths if os.path.exists(p)])
        hle_variables = {name: 0 for name in hle_var_names}
        if args.hle_symbols and hle_var_names:
            # Functions too: an implementation may import a function's
            # address to read its code (see hle.resolve_variables).
            hle_variables, var_notes = resolve_variables(
                hle_var_names, load_variables(args.hle_symbols)
                + load_symbols(args.hle_symbols))
            print(f"XDK symbols imported by name: "
                  f"{sum(1 for v in hle_variables.values() if v)} of "
                  f"{len(hle_var_names)}", file=sys.stderr)
            for note in var_notes:
                print(f"  hle: {note}", file=sys.stderr)
        if args.hle_symbols:
            from .config import va_to_file_offset
            from .hle import implemented_names, load_symbols, plan, stack_cleanup
            xbe_bytes = translator.translator.xbe_data

            def _cleanup(addr):
                # Argument bytes the function's own `ret N` pops, read from the
                # binary: the thunk pops exactly what the lifted body would.
                info = translator.func_db.get(addr) or {}
                end = info.get("end")
                if isinstance(end, str):
                    end = int(end, 16)
                off = va_to_file_offset(addr)
                if not end or end <= addr or off is None or not xbe_bytes:
                    return None
                return stack_cleanup(xbe_bytes[off:off + (end - addr)], addr)
            impl_paths = args.hle_impl or [os.path.join(
                os.path.dirname(os.path.dirname(os.path.dirname(
                    os.path.abspath(__file__)))), "src", "hle")]
            implemented = implemented_names(
                [p for p in impl_paths if os.path.exists(p)])
            hle_replace, hle_notes = plan(load_symbols(args.hle_symbols),
                                          implemented, set(translator.func_db),
                                          manual, _cleanup)
            for addr in hle_replace:
                translator.func_db[addr]["name"] = f"sub_{addr:08X}"
            manual |= set(hle_replace)
            print(f"XDK functions replaced by name: {len(hle_replace)} of "
                  f"{len(implemented)} implemented", file=sys.stderr)
            for note in hle_notes:
                print(f"  hle: {note}", file=sys.stderr)

        # Replacements that run the title's own body first (HLE_ORIGINAL):
        # those bodies are still lifted, under their own name. Scanned whether
        # or not there is a symbols file, like the imported variables, because
        # the implementations declare the pointers either way.
        from .hle import keep_originals, wanted_originals
        wanted = wanted_originals([p for p in hle_impl_paths if os.path.exists(p)])
        hle_keep = keep_originals(hle_replace, wanted)

        if args.only_hle_thunks:
            # Refresh recomp_hle.c alone. Which original bodies exist is read
            # from the generated sources rather than from this run's function
            # list, because nothing is being translated: the bodies that are
            # there are the ones the last real lift emitted, and claiming any
            # other would fail the link this is here to fix.
            import glob as _glob
            import re as _re
            have = set()          # bodies kept for HLE_ORIGINAL
            emitted = set()       # ordinary bodies this lift already wrote
            body_re = _re.compile(r"^void sub_([0-9A-F]{8})_hle_original\(void\)",
                                  _re.M)
            plain_re = _re.compile(r"^void sub_([0-9A-F]{8})\(void\)", _re.M)
            for path in _glob.glob(os.path.join(gen_dir, "*.c")):
                if os.path.basename(path) == "recomp_hle.c":
                    continue
                with open(path, encoding="utf-8", errors="replace") as fh:
                    text = fh.read()
                have.update(int(m, 16) for m in body_re.findall(text))
                emitted.update(int(m, 16) for m in plain_re.findall(text))

            # A name src/hle has gained since this title was lifted refers to a
            # function the old lift wrote out normally. Emitting a thunk for it
            # too defines sub_XXXXXXXX twice and fails the link -- which is the
            # failure this mode exists to prevent. Leave those to the caller's
            # next real lift: the title links and behaves exactly as it did,
            # and the count below says what it is missing out on.
            deferred = sorted(set(hle_replace) & emitted)
            for addr in deferred:
                del hle_replace[addr]

            # The dispatch table is the authority on what must exist: it was
            # written by the lift and names every address the title can call.
            # Anything it names that neither an ordinary body nor this plan's
            # thunks define would fail the link.
            #
            # That happens when src/hle has *lost* a name since the lift --
            # the body was kept only as sub_X_hle_original and the thunk was
            # the sole definition of sub_X. Mortal Kombat hit exactly this:
            # it was lifted while D3DDevice_SetPalette was an HLE_EXPORT on a
            # branch that is not merged, so on main nothing replaces that
            # address and nothing defines it either.
            #
            # Checking the dispatch table rather than the previous
            # recomp_hle.c matters, because this mode rewrites that file: run
            # twice, and comparing against it compares against your own last
            # answer. The first version did, and reported nothing wrong.
            #
            # Refuse rather than write. This exists to get a title building
            # again, and a refresh that breaks one differently is worse than
            # no refresh, so the current recomp_hle.c is left alone.
            out_path = os.path.join(gen_dir, "recomp_hle.c")
            dispatch = os.path.join(gen_dir, "recomp_dispatch.c")
            if os.path.exists(dispatch):
                with open(dispatch, encoding="utf-8", errors="replace") as fh:
                    needed = {int(m, 16) for m in
                              _re.findall(r"\(recomp_func_t\)sub_([0-9A-F]{8})",
                                          fh.read())}
                lost = sorted(needed - emitted - set(hle_replace) - set(manual))
                if lost:
                    print(f"Not rewriting {out_path}: the dispatch table names "
                          f"{len(lost)} function(s) that nothing would define "
                          f"(e.g. 0x{lost[0]:08X}). src/hle has changed which "
                          f"addresses it replaces since this title was lifted "
                          f"-- lift it again.", file=sys.stderr)
                    sys.exit(1)
            hle_originals = {name: None for name in wanted}
            for addr, (name, _p, _r) in hle_replace.items():
                if name in wanted and addr in have:
                    hle_originals[name] = addr
            from .hle import render_thunks
            out = out_path
            with open(out, "w", encoding="utf-8") as fh:
                fh.write(render_thunks(hle_replace, hle_variables, hle_originals))
            # The runtime header travels with the generated code, and a real
            # lift refreshes it every run for the reason translator.py spells
            # out at length: the lifter and this header are two halves of one
            # contract, and a stale copy gives a link error against generated
            # code that is perfectly correct. Refreshing thunks without it
            # reproduces exactly that -- Max Payne and Mortal Kombat both
            # failed on an unresolved g_icall_saved_esp from a header left
            # behind by a branch that is not checked out.
            types_dst = os.path.join(gen_dir, "recomp_types.h")
            types_src = os.path.join(os.path.dirname(__file__), "..", "..",
                                     "templates", "runtime", "recomp_types.h")
            try:
                with open(types_src, encoding="utf-8") as fh:
                    want = fh.read()
                have = None
                if os.path.exists(types_dst):
                    with open(types_dst, encoding="utf-8") as fh:
                        have = fh.read()
                if have != want:
                    with open(types_dst, "w", encoding="utf-8") as fh:
                        fh.write(want)
                    print("  refreshed recomp_types.h (runtime register model)",
                          file=sys.stderr)
            except OSError as exc:
                print(f"  warning: could not refresh recomp_types.h: {exc}",
                      file=sys.stderr)

            kept = sum(1 for v in hle_originals.values() if v is not None)
            print(f"Rewrote {out}: {len(hle_replace)} replacements, "
                  f"{kept} of {len(wanted)} original bodies already lifted",
                  file=sys.stderr)
            if deferred:
                print(f"  {len(deferred)} replacement(s) new since this title "
                      f"was lifted are not active until it is lifted again; "
                      f"the old body still runs", file=sys.stderr)
            missing = sorted(n for n, v in hle_originals.items() if v is None
                             and n in {nm for _a, (nm, _p, _r)
                                       in hle_replace.items()})
            for name in missing:
                print(f"  {name} is replaced here but its body was not lifted; "
                      f"re-lift to run the title's own code for it",
                      file=sys.stderr)
            return 0
        # A body can only be kept if this lift emits it: translate_batch_split
        # drops owned function starts, and --game-only never passes some
        # categories in. Pointing recomp_hle.c at a body that is not there
        # fails the link, so such an original is left 0 instead, and the
        # replacement reports it at run time.
        lifted = ({item[0] for item in funcs}
                  - set(translator.translator.owned_function_starts))
        for addr in sorted(set(hle_keep) - lifted):
            name = hle_replace[addr][0]
            print(f"warning: {name} at 0x{addr:08X} is replaced but its body is "
                  f"not lifted in this run, so hle_original_{name} is 0",
                  file=sys.stderr)
            del hle_keep[addr]
        hle_originals = {name: None for name in wanted}
        for addr in hle_keep:
            hle_originals[hle_replace[addr][0]] = addr
        if wanted:
            print(f"Original bodies kept for replacements that run them: "
                  f"{len(hle_keep)} of {len(wanted)}", file=sys.stderr)

        stats = translator.translate_batch_split(
            funcs,
            output_dir=gen_dir,
            chunk_size=args.split,
            verbose=args.verbose,
            manual=manual,
            keep_bodies=hle_keep,
        )

        # Written after translation, which clears stale files from gen_dir, and
        # written even when empty, so a stale thunk file from an earlier lift
        # can never define a function this lift generated.
        from .hle import render_thunks
        with open(os.path.join(gen_dir, "recomp_hle.c"), "w",
                  encoding="utf-8") as fh:
            fh.write(render_thunks(hle_replace, hle_variables, hle_originals))

        t_translate = time.time() - t0
        print(f"\n=== Split Translation Complete ({t_translate:.1f}s) ===",
              file=sys.stderr)
        print(f"{stats['translated']}/{stats['total']} functions "
              f"({stats['failed']} failed), "
              f"{stats['total_lines']} lines of C, "
              f"{stats['num_chunks']} source files",
              file=sys.stderr)
        if stats.get("unresolved_stubs"):
            print(f"{stats['unresolved_stubs']} unresolved call targets stubbed "
                  f"(addresses called but not detected as functions)",
                  file=sys.stderr)
        _report_unimplemented(stats)
        for f_path in stats.get("files", []):
            print(f"  {f_path}", file=sys.stderr)
    else:
        stats = translator.translate_batch(
            funcs,
            max_funcs=args.max_funcs,
            verbose=args.verbose,
        )

        t_translate = time.time() - t0

        print(f"\n=== Translation Complete ({t_translate:.1f}s) ===",
              file=sys.stderr)
        print_stats(stats)

    # Write summary
    output_dir = args.output_dir or os.path.join(
        os.path.dirname(__file__), "output")
    summary_path = write_summary(stats, output_dir)
    print(f"\nSummary: {summary_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
