#!/usr/bin/env python3
"""
fsm_configurator.py

A Tkinter GUI to create flat FSM configurations, export an XML intermediate,
and generate a .h/.c pair from that XML.

No external dependencies. Uses Python stdlib only.
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog, simpledialog
import xml.etree.ElementTree as ET
from xml.dom import minidom
import datetime
import os

# -----------------------
# Helper: pretty xml
# -----------------------
def prettify_xml(elem):
    rough = ET.tostring(elem, 'utf-8')
    reparsed = minidom.parseString(rough)
    return reparsed.toprettyxml(indent="  ")

# -----------------------
# Data models (in memory)
# -----------------------
class StateDef:
    def __init__(self, name, flags="0", handler="", is_entry=False, allowedEvents=None):
        self.name = name.strip()
        self.flags = flags.strip()
        self.handler = handler.strip()
        self.is_entry = is_entry
        # allowedEvents is a list of event names (strings). Empty or None => allow all events.
        self.allowedEvents = list(allowedEvents) if allowedEvents else []


class EventDef:
    def __init__(self, name, value=None):
        self.name = name.strip()
        self.value = value

class FSMDef:
    def __init__(self, fsm_name):
        self.name = fsm_name.strip()
        self.context_name = f"{self.name}Context_T"
        self.states = []     # list of StateDef
        self.events = []     # list of EventDef
        self.unhandled_handler = ""
    def get_entry_state(self):
        for s in self.states:
            if s.is_entry:
                return s.name
        return None

# -----------------------
# XML generation / parse
# -----------------------
def fsm_to_xml(fsm: FSMDef):
    """
    Build an XML Element for the given FSMDef.

    - Always emits <AllowedEvents> per-state (empty means no restriction).
    - Only writes EventRef children for event names that exist in the FSM's Events list.
      This prevents accidental emission of stale / invalid names.
    """
    root = ET.Element('FSMConfiguration', attrib={
        'generated_on': datetime.datetime.utcnow().isoformat() + 'Z'
    })
    fsm_el = ET.SubElement(root, 'FSM', attrib={'name': fsm.name})
    ET.SubElement(fsm_el, 'ContextName').text = fsm.context_name

    # build a set of valid event names for this FSM (for quick lookup)
    valid_event_names = {e.name for e in fsm.events}

    states_el = ET.SubElement(fsm_el, 'States')
    for idx, s in enumerate(fsm.states):
        s_el = ET.SubElement(states_el, 'State', attrib={'id': str(idx)})
        ET.SubElement(s_el, 'Name').text = s.name
        ET.SubElement(s_el, 'Flags').text = s.flags
        ET.SubElement(s_el, 'Handler').text = s.handler
        ET.SubElement(s_el, 'IsEntry').text = '1' if s.is_entry else '0'

        # Always emit AllowedEvents to make debugging easier; empty element => allow all.
        allowed_el = ET.SubElement(s_el, 'AllowedEvents')

        # Only include event names that match an existing event in this FSM.
        # Also preserve the original order of s.allowedEvents (it represents designer intent).
        written_any = False
        for evname in getattr(s, 'allowedEvents', []) or []:
            if evname and evname in valid_event_names:
                ET.SubElement(allowed_el, 'EventRef').text = evname
                written_any = True

        # Optional: if the user selected some names that are not valid, we could warn them.
        # For now, silently ignore invalid names in the XML; they remain visible in the GUI.
        # If you'd prefer an explicit check/warning, uncomment the block below:
        #
        # invalids = [n for n in (getattr(s,'allowedEvents',[]) or []) if n and n not in valid_event_names]
        # if invalids:
        #     print(f"Warning: state '{s.name}' had invalid allowed event names: {invalids}")

    # Events
    events_el = ET.SubElement(fsm_el, 'Events')
    for idx, e in enumerate(fsm.events):
        e_el = ET.SubElement(events_el, 'Event', attrib={'id': str(idx)})
        ET.SubElement(e_el, 'Name').text = e.name
        if e.value is not None:
            ET.SubElement(e_el, 'Value').text = str(e.value)

    ET.SubElement(fsm_el, 'UnhandledHandler').text = fsm.unhandled_handler
    return root


def xml_to_fsm(xml_path):
    tree = ET.parse(xml_path)
    root = tree.getroot()
    fsm_el = root.find('FSM')
    name = fsm_el.attrib.get('name', 'UnnamedFSM')
    fsm = FSMDef(name)
    ctx = fsm_el.find('ContextName')
    if ctx is not None and ctx.text:
        fsm.context_name = ctx.text.strip()
    # states
    for s_el in fsm_el.find('States').findall('State'):
        nm = s_el.find('Name').text.strip()
        flags = s_el.find('Flags').text.strip() if s_el.find('Flags') is not None else "0"
        handler = s_el.find('Handler').text.strip() if s_el.find('Handler') is not None else ""
        is_entry = s_el.find('IsEntry').text.strip() == '1' if s_el.find('IsEntry') is not None else False
        # build StateDef
        state = StateDef(nm, flags, handler, is_entry)
        # AllowedEvents
        allowed_el = s_el.find('AllowedEvents')
        if allowed_el is not None:
            refs = [r.text.strip() for r in allowed_el.findall('EventRef') if r.text]
            state.allowedEvents = refs
        fsm.states.append(state)
    # events
    evs = fsm_el.find('Events')
    if evs is not None:
        for e_el in evs.findall('Event'):
            nm = e_el.find('Name').text.strip()
            v = e_el.find('Value')
            val = int(v.text) if (v is not None and v.text and v.text.strip().isdigit()) else None
            fsm.events.append(EventDef(nm, val))
    uh = fsm_el.find('UnhandledHandler')
    if uh is not None and uh.text:
        fsm.unhandled_handler = uh.text.strip()
    return fsm

# -----------------------
# Code templates
# -----------------------
HEADER_TEMPLATE = """\
/*
 * {header_name}
 * Generated by FSM Configurator on {date}
 *
 * NOTE: edit carefully. This file follows BATL coding conventions.
 */

#ifndef {guard}
#define {guard}

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "FSM_Types.h" /* expects FSMInstance_T, FSMStateAttr_T, FSMStateHandler_F etc. */

/* Forward-declare a typed context alias so handlers can use a named context type.
 * We alias the standard FSMContext_T to the per-FSM context name so code can
 * use <FsmName>Context_T while it remains compatible with FSM_Types.h.
 */
typedef FSMContext_T {context_name};

/* FSM context declaration */
extern FSMContext_T gst_{fsm_var_name}Context;

/* FSM instance declaration */
extern FSMInstance_T gst_{fsm_var_name};

/* State enum */
typedef enum {{
{state_enum}
}} {fsm_name}State_E;

/* Event enum */
typedef enum {{
{event_enum}
}} {fsm_name}EventID_E;

/* State handler prototypes (match FSMStateHandler_F) */
{handler_protos}

/* Attributes array */
extern const FSMStateAttr_T gstar_{fsm_name}StateAttrs[];

/* Unhandled event callback prototype (matches FSMStateHandler_F) */
bool {unhandled_handler}(FSMContext_T *stpt_FSMContext, const FSMEvent_T *stpt_event);

#endif /* {guard} */
"""


SOURCE_TEMPLATE = """\
/*
 * {source_name}
 * Generated by FSM Configurator on {date}
 */

#include "{header_file}"

/* Provide a file-local FSM context instance (application can extend or map its data) */
FSMContext_T gst_{fsm_var_name}Context = {{
    .vpt_customData = NULL
}};

/* FSM instance (extern in header) */
FSMInstance_T gst_{fsm_var_name} = {{
    .stpt_FSMContext    = &gst_{fsm_var_name}Context,
    .stpt_currentState  = {entry_state_ptr},
    .fpt_unhandledEventCb = {unhandled_handler_expr},
}};

/* State handler implementations (stubs matching FSMStateHandler_F) */
{handler_impls}

/* Allowed-event arrays (one per state that restricts events). */
{allowed_event_arrays}

/* State attributes array */
const FSMStateAttr_T gstar_{fsm_name}StateAttrs[] = {{
{state_flags_array}
}};
"""



# -----------------------
# Code generation functions
# -----------------------
def generate_header_text(fsm: FSMDef, header_name):
    guard = header_name.upper().replace('.', '_') + '_'
    fsm_var_name = f"{fsm.name}"
    # State enum lines
    state_enum_lines = []
    for idx, s in enumerate(fsm.states):
        state_enum_lines.append(f"    {fsm.name.upper()}_STATE_{s.name.upper()} = {idx},")
    state_enum = "\n".join(state_enum_lines) if state_enum_lines else "    /* no states */"

    # Event enum
    event_enum_lines = []
    for idx, e in enumerate(fsm.events):
        if e.value is not None:
            event_enum_lines.append(f"    {fsm.name.upper()}_EVENT_{e.name.upper()} = {e.value},")
        else:
            event_enum_lines.append(f"    {fsm.name.upper()}_EVENT_{e.name.upper()} = {idx},")
    event_enum = "\n".join(event_enum_lines) if event_enum_lines else "    /* no events */"

    # Handlers: use signature matching FSMStateHandler_F
    handler_protos_lines = []
    for s in fsm.states:
        prot = f"extern bool gb_{s.handler}(FSMContext_T *stpt_FSMContext, const FSMEvent_T *stpt_event);"
        handler_protos_lines.append(prot)
    handler_protos = "\n".join(handler_protos_lines) if handler_protos_lines else "/* no handlers */"

    content = HEADER_TEMPLATE.format(
        header_name=header_name,
        date=datetime.date.today().isoformat(),
        guard=guard,
        context_name=fsm.context_name,
        fsm_var_name=fsm_var_name,
        fsm_name=fsm.name,
        state_enum=state_enum,
        event_enum=event_enum,
        handler_protos=handler_protos,
        unhandled_handler=(fsm.unhandled_handler or f"{fsm.name}_UnhandledEvent")
    )
    return content

def _sanitize_ident(name: str) -> str:
    # Convert arbitrary name into a safe C identifier fragment
    # Keep alnum and underscore, replace others with underscore
    return ''.join(ch if (ch.isalnum() or ch == '_') else '_' for ch in name)

def generate_source_text(fsm: FSMDef, header_file, source_name):
    """
    Generate .c contents:
      - emit static allowed-event arrays for states that have allowedEvents
      - initialize FSMStateAttr_T entries with .u32pt_allowedEventIDs and .u8_allowedEventCount
    """
    fsm_var_name = f"{fsm.name}"

    # Find entry state index (default 0)
    entry_index = 0
    for idx, s in enumerate(fsm.states):
        if getattr(s, "is_entry", False):
            entry_index = idx
            break

    entry_state_ptr = f"&gstar_{fsm.name}StateAttrs[{entry_index}]"
    unhandled_handler_expr = f"{fsm.unhandled_handler}" if fsm.unhandled_handler else "NULL"

    # Handler impls (stubs)
    handler_impls_lines = []
    for s in fsm.states:
        provided = (s.handler or "").strip()
        if provided:
            stub_name = provided
        else:
            safe_state = _sanitize_ident(s.name)
            stub_name = f"{fsm.name}_State_{safe_state}_Handler"
        impl = (
            f"bool gb_{stub_name}(FSMContext_T *stpt_FSMContext, const FSMEvent_T *stpt_event) {{\n"
            f"    /* TODO: implement handler for state {s.name} */\n"
            f"    (void)stpt_FSMContext;\n"
            f"    (void)stpt_event;\n"
            f"    return false; /* return true if event handled */\n"
            f"}}"
        )
        handler_impls_lines.append(impl)
    handler_impls = "\n\n".join(handler_impls_lines) if handler_impls_lines else "/* no handlers */"

    # Build allowed-event arrays and map per-state array names/counts
    allowed_array_blocks = []
    state_allowed_meta = []  # list of tuples (array_name or None, count)
    for s in fsm.states:
        if getattr(s, "allowedEvents", None):
            # Build array name using sanitized FSM+State
            safe_state = _sanitize_ident(s.name)
            array_name = f"{fsm.name}_AllowedEvents_{safe_state}"
            # Convert event names to enum tokens (use header enum names)
            enum_tokens = []
            for evname in s.allowedEvents:
                token = f"{fsm.name.upper()}_EVENT_{_sanitize_ident(evname).upper()}"
                enum_tokens.append(token)
            tokens_text = ", ".join(enum_tokens)
            block = f"static const uint32_t {array_name}[] = {{ {tokens_text} }};"
            allowed_array_blocks.append(block)
            state_allowed_meta.append((array_name, len(enum_tokens)))
        else:
            state_allowed_meta.append((None, 0))

    allowed_event_arrays = "\n".join(allowed_array_blocks) if allowed_array_blocks else "/* no allowed-event arrays */"

    # Build state attributes initializers, injecting allowed events fields
    state_flags_lines = []
    for idx, s in enumerate(fsm.states):
        handler_field = s.handler.strip() if (s.handler and s.handler.strip()) else "NULL"
        flags_val = s.flags.strip() if (s.flags is not None) else "0"
        if flags_val == "":
            flags_val = "0"
        array_name, count = state_allowed_meta[idx]
        if array_name:
            allowed_init = f".u32pt_allowedEventIDs = {array_name}, .u8_allowedEventCount = {count}, "
        else:
            allowed_init = ".u32pt_allowedEventIDs = NULL, .u8_allowedEventCount = 0, "
        # Compose initializer; keep field order consistent with FSMStateAttr_T
        state_flags_lines.append(
            f"    /* {s.name} */ {{ .fpt_handler = gb_{handler_field}, {allowed_init}.u32_flags = {flags_val}, .u8_stateID = {idx} }},"
        )

    state_flags_array = "\n".join(state_flags_lines) if state_flags_lines else "    /* empty */"

    content = SOURCE_TEMPLATE.format(
        source_name=source_name,
        date=datetime.date.today().isoformat(),
        header_file=header_file,
        fsm_var_name=fsm_var_name,
        entry_state_ptr=entry_state_ptr,
        unhandled_handler_expr=unhandled_handler_expr,
        handler_impls=handler_impls,
        allowed_event_arrays=allowed_event_arrays,
        fsm_name=fsm.name,
        state_flags_array=state_flags_array
    )
    return content

# -----------------------
# Tkinter GUI
# -----------------------
class FSMConfiguratorApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("FSM Configurator")
        self.geometry("1000x650")
        self.fsms = []  # list of FSMDef
        self.current_fsm = None

        # new: configure appearance
        try:
            self.configure_style()
        except Exception:
            pass

        self.create_widgets()

    def configure_style(self):
        """Configure ttk styles and fonts for a cleaner, professional look."""
        from tkinter import font as tkfont

        style = ttk.Style(self)

        # Try a friendly theme. 'clam' is widely available and style-friendly.
        try:
            style.theme_use('clam')
        except Exception:
            # fallback to default if clam is not present
            pass

        # Base fonts
        default_font = tkfont.nametofont("TkDefaultFont")
        default_font.configure(size=10, family="Segoe UI", weight="normal")
        heading_font = (default_font.actual('family'), 13, 'bold')
        monospace_font = ("Consolas", 10)

        # Frame and label backgrounds
        style.configure('TFrame', background='#f5f7fa')
        style.configure('TLabel', background='#f5f7fa', font=default_font)
        style.configure('Header.TLabel', background='#f5f7fa', font=heading_font, foreground='#173A5E')

        # LabelFrame styling
        style.configure('TLabelframe', background='#ffffff', borderwidth=1, relief='solid')
        style.configure('TLabelframe.Label', font=('Segoe UI', 11, 'bold'), foreground='#173A5E')

        # Buttons
        style.configure('TButton', padding=(6, 4), font=default_font)
        style.configure('Accent.TButton', padding=(6, 4), font=default_font, foreground='white', background='#2B7CFF')
        # Map for hover/pressed state (works with clam)
        style.map('Accent.TButton',
                  background=[('active', '#1a61d9'), ('pressed', '#164fb8')],
                  foreground=[('disabled', '#888888')])

        # Treeview
        style.configure('Treeview',
                        font=monospace_font,
                        rowheight=22,
                        fieldbackground='#ffffff',
                        background='#ffffff')
        style.configure('Treeview.Heading', font=('Segoe UI', 10, 'bold'), background='#e9f0ff')

        # Combobox
        style.configure('TCombobox', padding=4, font=default_font)

        # Entry
        style.configure('TEntry', padding=4, font=default_font)

        # Misc spacing constants (used below)
        self._padx = 10
        self._pady = 8


    def create_widgets(self):
        # Call configure_style first if not already called (safe to call again)
        if not hasattr(self, "_padx"):
            try:
                self.configure_style()
            except Exception:
                # don't fail if style configuration has an issue
                self._padx = 8
                self._pady = 6

        # Top header (replace your existing header block with this)
        header = ttk.Frame(self, padding=(self._padx, self._pady))
        header.pack(fill=tk.X)

        # Left: title / subtitle
        header_left = ttk.Frame(header)
        header_left.pack(side=tk.LEFT, anchor=tk.W)
        ttk.Label(header_left, text="FSM Configurator", style='Header.TLabel').pack(anchor=tk.W)
        ttk.Label(header_left, text="Design flat FSMs → Export XML and generate .h/.c files",
                  font=('Segoe UI', 9), foreground='#4a4a4a').pack(anchor=tk.W, pady=(2, 6))

        # Right: persistent toolbar with Import / Export / Generate so they're always visible
        header_right = ttk.Frame(header)
        header_right.pack(side=tk.RIGHT, anchor=tk.E)
        # Import button (new)
        ttk.Button(header_right, text="Import XML", command=self.import_xml_dialog).pack(side=tk.LEFT, padx=(0, 8))
        # Export / Generate (existing)
        ttk.Button(header_right, text="Export XML", command=self.export_xml, style='Accent.TButton')\
            .pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(header_right, text="Generate .h/.c", command=self.generate_code_from_xml_dialog, style='Accent.TButton')\
            .pack(side=tk.LEFT)


        main = ttk.Frame(self)
        main.pack(fill=tk.BOTH, expand=True, padx=self._padx, pady=(0, self._pady))

        # Left panel (FSM list)
        left = ttk.Frame(main)
        left.pack(side=tk.LEFT, fill=tk.Y, padx=(0, self._padx), pady=0)

        ttk.Label(left, text="FSM Instances").pack(anchor=tk.W)
        self.fsm_listbox = tk.Listbox(left, height=14, width=28, relief='flat', bd=0, highlightthickness=1)
        self.fsm_listbox.pack(fill=tk.X, pady=(6, 6))
        self.fsm_listbox.bind("<<ListboxSelect>>", self.on_select_fsm)

        btn_frame = ttk.Frame(left)
        btn_frame.pack(fill=tk.X, pady=(4, 8))
        ttk.Button(btn_frame, text="New FSM", command=self.new_fsm).pack(side=tk.LEFT, padx=(0,6))
        ttk.Button(btn_frame, text="Delete FSM", command=self.delete_fsm).pack(side=tk.LEFT, padx=(0,6))
        ttk.Button(btn_frame, text="Rename FSM", command=self.rename_fsm).pack(side=tk.LEFT)

        # Middle panel (details)
        center = ttk.Frame(main, relief='flat')
        center.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Meta frame
        meta = ttk.LabelFrame(center, text="FSM Meta", padding=(10,8))
        meta.pack(fill=tk.X, padx=(0, self._padx), pady=(0, 10))

        ttk.Label(meta, text="FSM Name:").grid(row=0, column=0, sticky=tk.W, padx=(4,10), pady=4)
        self.fsm_name_var = tk.StringVar()
        entry_name = ttk.Entry(meta, textvariable=self.fsm_name_var, width=42)
        entry_name.grid(row=0, column=1, padx=4, pady=4, sticky=tk.W)

        ttk.Label(meta, text="Context name:").grid(row=1, column=0, sticky=tk.W, padx=(4,10), pady=4)
        self.context_var = tk.StringVar()
        entry_ctx = ttk.Entry(meta, textvariable=self.context_var, width=42)
        entry_ctx.grid(row=1, column=1, padx=4, pady=4, sticky=tk.W)

        ttk.Button(meta, text="Apply Meta", command=self.apply_meta).grid(row=0, column=2, rowspan=2, padx=8, sticky=tk.E)

        # Entry state combobox
        ttk.Label(meta, text="Entry State:").grid(row=2, column=0, sticky=tk.W, padx=(4,10), pady=6)
        self.entry_state_var = tk.StringVar()
        self.entry_state_combobox = ttk.Combobox(meta, textvariable=self.entry_state_var, state='readonly', width=40)
        self.entry_state_combobox.grid(row=2, column=1, padx=4, pady=6, sticky=tk.W)
        self.entry_state_combobox.bind("<<ComboboxSelected>>", lambda e: self.on_entry_state_selected())

        # States frame
        states_frame = ttk.LabelFrame(center, text="States", padding=(8,8))
        states_frame.pack(fill=tk.BOTH, expand=True, padx=(0, self._padx), pady=(0, 10))
        sf_top = ttk.Frame(states_frame)
        sf_top.pack(fill=tk.X, padx=4, pady=(0,6))
        ttk.Button(sf_top, text="Add State", command=self.add_state).pack(side=tk.LEFT)
        ttk.Button(sf_top, text="Remove State", command=self.remove_state).pack(side=tk.LEFT, padx=6)
        ttk.Button(sf_top, text="Mark Entry", command=self.mark_entry_state).pack(side=tk.LEFT, padx=6)

        self.states_tree = ttk.Treeview(
            states_frame,
            columns=("state_name", "flags", "handler", "is_entry"),
            show='headings',
            selectmode='browse',
            height=8
        )
        self.states_tree.heading('state_name', text='State Name')
        self.states_tree.heading('flags', text='Flags')
        self.states_tree.heading('handler', text='Handler')
        self.states_tree.heading('is_entry', text='Entry')
        self.states_tree.column('state_name', width=200)
        self.states_tree.column('flags', width=80, anchor='center')
        self.states_tree.column('handler', width=260)
        self.states_tree.column('is_entry', width=60, anchor='center')
        self.states_tree.pack(fill=tk.BOTH, expand=True, padx=6, pady=(4,2))
        self.states_tree.bind("<Double-1>", self.edit_state)

        # Events frame
        events_frame = ttk.LabelFrame(center, text="Events", padding=(8,8))
        events_frame.pack(fill=tk.BOTH, expand=False, padx=(0, self._padx), pady=(0, 10))
        ev_top = ttk.Frame(events_frame)
        ev_top.pack(fill=tk.X, padx=4, pady=(0,6))
        ttk.Button(ev_top, text="Add Event", command=self.add_event).pack(side=tk.LEFT)
        ttk.Button(ev_top, text="Remove Event", command=self.remove_event).pack(side=tk.LEFT, padx=6)

        self.events_tree = ttk.Treeview(events_frame, columns=("value",), show='headings', selectmode='browse', height=6)
        self.events_tree.heading('value', text='Value (optional)')
        self.events_tree.column('value', width=120, anchor='center')
        self.events_tree.pack(fill=tk.BOTH, expand=True, padx=6, pady=(4,2))
        self.events_tree.bind("<Double-1>", self.edit_event)

        # Bottom controls
        bottom = ttk.Frame(center)
        bottom.pack(fill=tk.X, padx=(0, self._padx), pady=(6,0))
        ttk.Label(bottom, text="Unhandled Event Handler:").pack(side=tk.LEFT, padx=(2,6))
        self.unhandled_var = tk.StringVar()
        ttk.Entry(bottom, textvariable=self.unhandled_var, width=36).pack(side=tk.LEFT, padx=(0,8))
        ttk.Button(bottom, text="Export XML", command=self.export_xml).pack(side=tk.LEFT, padx=(4,6))
        ttk.Button(bottom, text="Generate C/H from XML", command=self.generate_code_from_xml_dialog).pack(side=tk.LEFT)

        # Status / hint bar at the bottom
        status = ttk.Frame(self, padding=(self._padx//2, 6))
        status.pack(fill=tk.X, side=tk.BOTTOM)
        ttk.Label(status, text="Tip: Double-click a state or event to edit. Use 'Entry State' dropdown to set the entry.", font=('Segoe UI', 9), foreground='#555').pack(anchor=tk.W)


    # -----------------------
    # FSM list operations
    # -----------------------
    def new_fsm(self):
        name = simpledialog.askstring("New FSM", "Enter FSM name (identifier):", parent=self)
        if not name:
            return
        if any(f.name == name for f in self.fsms):
            messagebox.showerror("Error", "FSM with that name already exists.")
            return
        f = FSMDef(name)
        self.fsms.append(f)
        self.fsm_listbox.insert(tk.END, name)
        self.fsm_listbox.selection_clear(0, tk.END)
        self.fsm_listbox.selection_set(tk.END)
        self.on_select_fsm()

    def delete_fsm(self):
        sel = self.fsm_listbox.curselection()
        if not sel:
            return
        idx = sel[0]
        name = self.fsm_listbox.get(idx)
        if messagebox.askyesno("Confirm", f"Delete FSM '{name}'?"):
            self.fsm_listbox.delete(idx)
            del self.fsms[idx]
            self.current_fsm = None
            self.clear_detail_views()

    def rename_fsm(self):
        sel = self.fsm_listbox.curselection()
        if not sel:
            return
        idx = sel[0]
        old = self.fsms[idx].name
        name = simpledialog.askstring("Rename FSM", "New name:", initialvalue=old, parent=self)
        if not name:
            return
        self.fsms[idx].name = name
        self.fsm_listbox.delete(idx)
        self.fsm_listbox.insert(idx, name)
        self.fsm_listbox.selection_set(idx)
        self.on_select_fsm()

    def on_select_fsm(self, event=None):
        """
        Populate UI based on selected FSM.

        Behavior change: if the listbox has no selection but self.current_fsm
        is still a valid FSM in self.fsms, keep using it (do not clear the UI).
        This prevents the FSM from appearing "unselected" when combobox/button
        interactions briefly clear listbox selection.
        """
        sel = self.fsm_listbox.curselection()

        # Determine which FSM to use:
        if sel and len(sel) > 0:
            # Normal case: user selected an FSM in the listbox
            idx = int(sel[0])
            # Guard against out-of-range indices
            if 0 <= idx < len(self.fsms):
                self.current_fsm = self.fsms[idx]
            else:
                # Invalid index -> clear view
                self.current_fsm = None
                self.clear_detail_views()
                try:
                    self.entry_state_combobox['values'] = []
                    self.entry_state_var.set("")
                except Exception:
                    pass
                return
        else:
            # No listbox selection: if we already have a valid current_fsm, keep it
            if getattr(self, "current_fsm", None) is not None and self.current_fsm in self.fsms:
                # re-select it in the listbox to keep UI consistent
                try:
                    idx = self.fsms.index(self.current_fsm)
                    self.fsm_listbox.selection_clear(0, tk.END)
                    self.fsm_listbox.selection_set(idx)
                except Exception:
                    pass
            else:
                # Nothing to show
                self.current_fsm = None
                self.clear_detail_views()
                try:
                    self.entry_state_combobox['values'] = []
                    self.entry_state_var.set("")
                except Exception:
                    pass
                return

        # At this point, self.current_fsm is a valid FSM to display
        # populate meta
        self.fsm_name_var.set(self.current_fsm.name)
        self.context_var.set(self.current_fsm.context_name)
        self.unhandled_var.set(self.current_fsm.unhandled_handler)

        # Update entry state display (only that state's name should be shown)
        entry_name = self.current_fsm.get_entry_state()
        self.entry_state_var.set(entry_name if entry_name is not None else "")

        # states
        for i in self.states_tree.get_children():
            self.states_tree.delete(i)
        state_names = []
        for s in self.current_fsm.states:
            # Insert values in the exact column order: state_name, flags, handler, is_entry
            self.states_tree.insert('', tk.END, values=(s.name, s.flags, s.handler, 'yes' if s.is_entry else ''))
            state_names.append(s.name)

        # populate combobox values with available states and select current entry (if any)
        try:
            self.entry_state_combobox['values'] = state_names
            if entry_name in state_names:
                self.entry_state_combobox.current(state_names.index(entry_name))
            else:
                # if no entry or it's not in list, clear combobox selection
                self.entry_state_var.set("")
        except Exception:
            # ignore combobox errors silently (keeps backward compatibility)
            pass

        # events
        for i in self.events_tree.get_children():
            self.events_tree.delete(i)
        for e in self.current_fsm.events:
            self.events_tree.insert('', tk.END, values=(e.name, e.value if e.value is not None else ""))


    def clear_detail_views(self):
        self.fsm_name_var.set("")
        self.context_var.set("")
        self.unhandled_var.set("")
        for i in self.states_tree.get_children():
            self.states_tree.delete(i)
        for i in self.events_tree.get_children():
            self.events_tree.delete(i)

    def apply_meta(self):
        if not self.current_fsm:
            messagebox.showerror("Error", "No FSM selected.")
            return
        new_name = self.fsm_name_var.get().strip()
        if not new_name:
            messagebox.showerror("Error", "FSM name cannot be empty.")
            return
        # update name in listbox
        idx = self.fsms.index(self.current_fsm)
        self.current_fsm.name = new_name
        # context
        ctx = self.context_var.get().strip()
        if ctx:
            self.current_fsm.context_name = ctx
        # unhandled
        self.current_fsm.unhandled_handler = self.unhandled_var.get().strip()
        self.fsm_listbox.delete(idx)
        self.fsm_listbox.insert(idx, new_name)
        self.fsm_listbox.selection_set(idx)
        self.on_select_fsm()

    # -----------------------
    # States / Events ops
    # -----------------------
    def add_state(self):
        """
        Robust add_state replacement.

        Resolves the FSM instance to modify using:
         - current listbox selection (preferred)
         - fallback to self.current_fsm if it points to a valid entry in self.fsms
         - fallback to the single FSM if only one exists (convenience)

        Uses a local `target_fsm` variable to avoid relying on self.current_fsm
        during the whole operation (defensive against unexpected UI changes).
        """

        # Helper: try to resolve a valid FSM object from UI state
        def _resolve_target_fsm():
            # 1) try the listbox selection first
            sel = self.fsm_listbox.curselection()
            if sel:
                try:
                    idx = int(sel[0])
                    if 0 <= idx < len(self.fsms):
                        return self.fsms[idx]
                except Exception:
                    pass
            # 2) if self.current_fsm appears valid and still in list, use it
            if getattr(self, "current_fsm", None) is not None:
                if self.current_fsm in self.fsms:
                    return self.current_fsm
            # 3) if exactly one FSM exists, use it
            if len(self.fsms) == 1:
                return self.fsms[0]
            return None

        target_fsm = _resolve_target_fsm()
        if target_fsm is None:
            messagebox.showerror("Error", "No FSM selected. Create/select an FSM first before adding states.")
            return

        # --- Gather inputs (treat None as 'cancel' and accept "0" as valid value) ---
        nm = simpledialog.askstring("State Name", "Enter state identifier (no spaces):", parent=self)
        if nm is None:
            # user cancelled dialog
            return
        nm = nm.strip()
        if nm == "":
            messagebox.showerror("Error", "State name cannot be empty.")
            return

        handler = simpledialog.askstring("Handler Name", "Enter state handler function name (e.g., MyFsm_StateIdleHandler):", parent=self)
        if handler is None:
            # user cancelled
            return
        handler = handler.strip() or f"{nm}_Handler"

        flags = simpledialog.askstring("State Flags", "Flags value (numeric) (optional):", parent=self, initialvalue="0")
        if flags is None:
            # user cancelled
            return
        flags = flags.strip()
        if flags == "":
            flags = "0"

        # Final sanity check before modifying model
        if target_fsm is None:
            # defensive - should not happen due to earlier check
            messagebox.showerror("Error", "Internal error: no FSM to add state to.")
            return

        # Append to the resolved FSM's states (use local variable, not self.current_fsm)
        try:
            # Use the target_fsm's event list for the dialog so selection is consistent
            allowed = self.pick_allowed_events_dialog(initial_selected=[], fsm=target_fsm)
            s = StateDef(nm, flags, handler, False, allowedEvents=allowed)
            s.allowedEvents = allowed
            target_fsm.states.append(s)
        except Exception as ex:
            messagebox.showerror("Error", f"Failed to add state: {ex}")
            return

        # If the resolved FSM differs from self.current_fsm, keep UI consistent:
        # set self.current_fsm to the target and select it in the listbox (if possible)
        try:
            if getattr(self, "current_fsm", None) is not target_fsm:
                # find its index and select it
                idx = self.fsms.index(target_fsm)
                self.fsm_listbox.selection_clear(0, tk.END)
                self.fsm_listbox.selection_set(idx)
                self.current_fsm = target_fsm
        except Exception:
            # ignore selection sync errors; still refresh UI for safety
            pass

        # Refresh UI to show the new state
        self.on_select_fsm()

    def remove_state(self):
        sel = self.states_tree.selection()
        if not sel:
            return
        item = sel[0]
        values = self.states_tree.item(item, 'values')
        nm = values[0]
        if messagebox.askyesno("Confirm", f"Remove state '{nm}'?"):
            self.current_fsm.states = [s for s in self.current_fsm.states if s.name != nm]
            self.on_select_fsm()

    def pick_allowed_events_dialog(self, initial_selected=None, fsm=None):
        """
        Show a modal dialog listing events for `fsm` (or self.current_fsm if fsm is None).
        Returns list of selected event names. If initial_selected is provided (list of names),
        pre-select those.
        """
        # Prefer provided fsm, otherwise fall back to current_fsm
        if fsm is None:
            fsm = getattr(self, "current_fsm", None)
        if not fsm:
            return []

        events = [e.name for e in fsm.events]
        initial_selected = initial_selected or []

        dlg = tk.Toplevel(self)
        dlg.title("Pick Allowed Events")
        dlg.transient(self)
        dlg.grab_set()
        dlg.resizable(False, False)

        ttk.Label(dlg, text="Select events that this state will accept (multi-select):").pack(padx=12, pady=(10, 6), anchor=tk.W)

        lb_frame = ttk.Frame(dlg)
        lb_frame.pack(padx=12, pady=(0, 8), fill=tk.BOTH, expand=True)
        listbox = tk.Listbox(lb_frame, selectmode=tk.MULTIPLE, height=min(12, max(4, len(events))))
        listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar = ttk.Scrollbar(lb_frame, orient=tk.VERTICAL, command=listbox.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        listbox.config(yscrollcommand=scrollbar.set)

        # populate
        for ev in events:
            listbox.insert(tk.END, ev)

        # pre-select initial items
        for i, ev in enumerate(events):
            if ev in initial_selected:
                listbox.selection_set(i)

        btn_frame = ttk.Frame(dlg)
        btn_frame.pack(fill=tk.X, padx=12, pady=(0, 12))
        result = {"selected": None}

        def on_ok():
            sels = listbox.curselection()
            chosen = [events[i] for i in sels]
            result["selected"] = chosen
            dlg.destroy()

        def on_cancel():
            result["selected"] = None
            dlg.destroy()

        ttk.Button(btn_frame, text="OK", command=on_ok).pack(side=tk.RIGHT, padx=(6,0))
        ttk.Button(btn_frame, text="Cancel", command=on_cancel).pack(side=tk.RIGHT)

        # center the dialog and wait
        self.update_idletasks()
        dlg.wait_window()
        # Return chosen list or empty list (note: cancel returns empty list to caller)
        return result["selected"] if result["selected"] is not None else []


    def edit_state(self, event):
        """
        Edit selected state: name, handler, flags, and entry flag.
        Activated by double-click on a state row.
        """
        if not getattr(self, "current_fsm", None):
            return

        sel = self.states_tree.selection()
        if not sel:
            return
        item = sel[0]
        values = self.states_tree.item(item, 'values')
        if not values:
            return

        # Primary attempt: find by exact state name in values[0]
        orig_name = values[0]
        s = next((x for x in self.current_fsm.states if x.name == orig_name), None)

        # Fallback: use tree index mapping if name lookup failed (robustness for duplicates)
        if s is None:
            try:
                row_index = self.states_tree.index(item)
                s = self.current_fsm.states[row_index]
            except Exception:
                messagebox.showerror("Error", "Failed to locate the selected state.")
                return

        # Prompt new name
        new_name = simpledialog.askstring("State Name", "Edit state identifier (no spaces):", initialvalue=s.name, parent=self)
        if new_name is None:
            # user cancelled
            return
        new_name = new_name.strip()
        if new_name == "":
            messagebox.showerror("Error", "State name cannot be empty.")
            return

        # Prevent duplicate state names (except this state itself)
        if any((other.name == new_name) and (other is not s) for other in self.current_fsm.states):
            messagebox.showerror("Error", f"A state named '{new_name}' already exists.")
            return

        # Prompt handler name
        new_handler = simpledialog.askstring("Handler Name", "Handler function name:", initialvalue=s.handler, parent=self)
        if new_handler is None:
            return
        new_handler = new_handler.strip() or f"{new_name}_Handler"

        # Prompt flags (allow "0")
        new_flags = simpledialog.askstring("State Flags", "Flags value (numeric) (optional):", initialvalue=s.flags or "0", parent=self)
        if new_flags is None:
            return
        new_flags = new_flags.strip()
        if new_flags == "":
            new_flags = "0"

        # Ask whether this should be entry state
        make_entry = messagebox.askyesno("Entry State", "Mark this state as entry state?")

        # allow editing allowed events
        new_allowed = self.pick_allowed_events_dialog(initial_selected=s.allowedEvents, fsm=self.current_fsm)

        if new_allowed is None:
            return
        else:
            s.allowedEvents = list(new_allowed)

        # Apply changes to the model
        s.name = new_name
        s.handler = new_handler
        s.flags = new_flags

        if make_entry:
            # unset others
            for other in self.current_fsm.states:
                other.is_entry = False
            s.is_entry = True
        else:
            # If user chose not to mark it entry, leave other flags as they are
            pass

        # After changing name, ensure UI remains selected on this FSM and refresh
        try:
            # keep selection on current FSM in the listbox
            if self.current_fsm in self.fsms:
                idx = self.fsms.index(self.current_fsm)
                self.fsm_listbox.selection_clear(0, tk.END)
                self.fsm_listbox.selection_set(idx)
        except Exception:
            pass

        # Refresh UI (this will update tree & combobox)
        self.on_select_fsm()


    def mark_entry_state(self):
        sel = self.states_tree.selection()
        if not sel:
            messagebox.showerror("Error", "Select a state first (double-click to edit).")
            return
        values = self.states_tree.item(sel[0], 'values')
        # state name is now in values[0]
        name = values[0]
        for s in self.current_fsm.states:
            s.is_entry = (s.name == name)
        # sync combobox (set selected entry)
        try:
            vals = list(self.entry_state_combobox['values'])
            if name in vals:
                self.entry_state_combobox.current(vals.index(name))
                self.entry_state_var.set(name)
        except Exception:
            pass
        # refresh UI
        self.on_select_fsm()

    def on_entry_state_selected(self):
        """
        Called when user selects an entry state from the combobox.
        Updates the model so exactly one state has is_entry=True, then refreshes UI.
        """
        if not getattr(self, "current_fsm", None):
            return
        selected = self.entry_state_var.get()
        if selected is None:
            return
        # Ensure only the selected state is marked entry
        for s in self.current_fsm.states:
            s.is_entry = (s.name == selected)
        # Refresh UI (will re-sync combobox and tree)
        self.on_select_fsm()


    def add_event(self):
        if not self.current_fsm:
            messagebox.showerror("Error", "No FSM selected.")
            return
        nm = simpledialog.askstring("Event Name", "Enter event identifier (no spaces):", parent=self)
        if not nm:
            return
        val = simpledialog.askstring("Event Value", "Optional explicit numeric value (leave empty for auto):", parent=self)
        ev = EventDef(nm, int(val) if (val and val.isdigit()) else None)
        self.current_fsm.events.append(ev)
        self.on_select_fsm()

    def remove_event(self):
        sel = self.events_tree.selection()
        if not sel:
            return
        item = sel[0]
        values = self.events_tree.item(item, 'values')
        nm = values[0]
        if messagebox.askyesno("Confirm", f"Remove event '{nm}'?"):
            self.current_fsm.events = [e for e in self.current_fsm.events if e.name != nm]
            self.on_select_fsm()

    def edit_event(self, event):
        """
        Edit selected event: name and optional numeric value.
        Activated by double-click on an event row.
        """
        if not getattr(self, "current_fsm", None):
            return

        sel = self.events_tree.selection()
        if not sel:
            return
        item = sel[0]
        values = self.events_tree.item(item, 'values')
        if not values:
            return

        orig_name = values[0]
        ev = next((x for x in self.current_fsm.events if x.name == orig_name), None)

        # Fallback by index if name lookup fails
        if ev is None:
            try:
                row_index = self.events_tree.index(item)
                ev = self.current_fsm.events[row_index]
            except Exception:
                messagebox.showerror("Error", "Failed to locate the selected event.")
                return

        # Prompt for new event name
        new_name = simpledialog.askstring("Event Name", "Edit event identifier (no spaces):", initialvalue=ev.name, parent=self)
        if new_name is None:
            return
        new_name = new_name.strip()
        if new_name == "":
            messagebox.showerror("Error", "Event name cannot be empty.")
            return

        # Prevent duplicate event names (except this event itself)
        if any((other.name == new_name) and (other is not ev) for other in self.current_fsm.events):
            messagebox.showerror("Error", f"An event named '{new_name}' already exists.")
            return

        # Prompt for numeric value (optional)
        initial_val = str(ev.value) if ev.value is not None else ""
        new_val = simpledialog.askstring("Event Value", "Numeric value or empty for auto:", initialvalue=initial_val, parent=self)
        if new_val is None:
            return
        new_val = new_val.strip()
        if new_val == "":
            new_value_parsed = None
        else:
            try:
                # Allow integers (positive/negative). Reject non-integers.
                new_value_parsed = int(new_val, 0)  # allow 0x.. hex too
            except Exception:
                messagebox.showerror("Error", f"Invalid numeric value: '{new_val}'")
                return

        # Apply changes
        ev.name = new_name
        ev.value = new_value_parsed

        # Refresh UI
        try:
            if self.current_fsm in self.fsms:
                idx = self.fsms.index(self.current_fsm)
                self.fsm_listbox.selection_clear(0, tk.END)
                self.fsm_listbox.selection_set(idx)
        except Exception:
            pass

        self.on_select_fsm()


    # -----------------------
    # Export / Generate
    # -----------------------
    def export_xml(self):
        if not self.current_fsm:
            messagebox.showerror("Error", "No FSM selected.")
            return
        # ensure last metadata applied
        self.apply_meta()
        fsm = self.current_fsm
        # update unhandled handler
        fsm.unhandled_handler = self.unhandled_var.get().strip()
        xml_root = fsm_to_xml(fsm)
        pretty = prettify_xml(xml_root)
        fname = filedialog.asksaveasfilename(title="Save FSM XML", defaultextension=".xml", filetypes=[("XML files", "*.xml")], initialfile=f"{fsm.name}.xml")
        if not fname:
            return
        with open(fname, 'w', encoding='utf-8') as fh:
            fh.write(pretty)
        messagebox.showinfo("Saved", f"XML exported to {fname}")

    def import_xml_dialog(self):
        """
        Import an FSM XML file and add/replace it in the GUI model.
        Behavior:
         - If the imported FSM's name already exists, ask whether to replace it.
         - If it does not exist, append as a new FSM.
         - Select the imported/updated FSM and refresh the UI.
        """
        xml_path = filedialog.askopenfilename(title="Select FSM XML to import", filetypes=[("XML files","*.xml")])
        if not xml_path:
            return
        # Parse XML into FSMDef using existing function
        try:
            imported_fsm = xml_to_fsm(xml_path)
        except Exception as ex:
            messagebox.showerror("Error", f"Failed to parse XML:\n{ex}")
            return

        # Check if FSM with same name exists
        existing = next((f for f in self.fsms if f.name == imported_fsm.name), None)
        if existing:
            # Ask user whether to replace
            resp = messagebox.askyesno("FSM exists",
                                       f"FSM named '{imported_fsm.name}' already exists.\n\n"
                                       "Do you want to replace the existing FSM with the imported one?\n\n"
                                       "Yes = replace (existing will be overwritten)\nNo = cancel import")
            if not resp:
                return
            # Replace existing in list and model
            idx = self.fsms.index(existing)
            self.fsms[idx] = imported_fsm
            # Update listbox entry (keep at same index)
            self.fsm_listbox.delete(idx)
            self.fsm_listbox.insert(idx, imported_fsm.name)
            # Select it
            self.fsm_listbox.selection_clear(0, tk.END)
            self.fsm_listbox.selection_set(idx)
            self.current_fsm = imported_fsm
            self.on_select_fsm()
            messagebox.showinfo("Imported", f"FSM '{imported_fsm.name}' was replaced from XML.")
            return

        # If not existing, append
        self.fsms.append(imported_fsm)
        self.fsm_listbox.insert(tk.END, imported_fsm.name)
        # Select new FSM
        last_idx = len(self.fsms) - 1
        self.fsm_listbox.selection_clear(0, tk.END)
        self.fsm_listbox.selection_set(last_idx)
        self.current_fsm = imported_fsm
        self.on_select_fsm()
        messagebox.showinfo("Imported", f"FSM '{imported_fsm.name}' was imported and added.")

    def generate_code_from_xml_dialog(self):
        xml_path = filedialog.askopenfilename(title="Select FSM XML", filetypes=[("XML files","*.xml")])
        if not xml_path:
            return
        try:
            fsm = xml_to_fsm(xml_path)
        except Exception as ex:
            messagebox.showerror("Error", f"Failed to parse XML: {ex}")
            return
        # ask where to save
        folder = filedialog.askdirectory(title="Select output folder for generated code")
        if not folder:
            return
        # header & source filenames
        header_file = f"{fsm.name}.h"
        source_file = f"{fsm.name}.c"
        header_path = os.path.join(folder, header_file)
        source_path = os.path.join(folder, source_file)
        # generate texts
        htxt = generate_header_text(fsm, header_file)
        stxt = generate_source_text(fsm, header_file, source_file)
        with open(header_path, 'w', encoding='utf-8') as fh:
            fh.write(htxt)
        with open(source_path, 'w', encoding='utf-8') as fs:
            fs.write(stxt)
        messagebox.showinfo("Done", f"Generated:\n{header_path}\n{source_path}")

# -----------------------
# Main
# -----------------------
def main():
    app = FSMConfiguratorApp()
    app.mainloop()

if __name__ == "__main__":
    main()
