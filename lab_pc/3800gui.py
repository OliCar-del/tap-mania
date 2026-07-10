import os
import shutil
import threading
import wave

import psutil  # Used to detect removable drives
import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from serial.tools import list_ports

# Default timing window values (in ms)
DEFAULT_WINDOWS = {
    "Perfect": 100,
    "Good": 150,
    "OK": 200,
    "Poor": 250,
}

# Initialize main window
root = tk.Tk()
root.title("tpmania Configuration Tool")
root.geometry("1000x800")

# Global variable for serial connection
serial_connection = None


# ========================= Serial Connection =========================
def refresh_serial_ports():
    """Scan and refresh available serial ports."""
    # Get list of all COM ports on system
    ports = serial.tools.list_ports.comports()
    port_list = [port.device for port in ports]
    combo_serial["values"] = port_list
    # Automatically select the first port if available
    if port_list:
        combo_serial.current(0)


def connect_serial():
    """Connect to the selected serial port."""
    global serial_connection
    port = combo_serial.get()

    try:
        # Attempt to open serial port at 115200 baud rate
        serial_connection = serial.Serial(port, 115200, timeout=1)
        label_status.config(text="Connected", fg="green")
    except Exception as e:
        # Display error if port cannot be opened
        messagebox.showerror("Connection Failed", f"Unable to open port:\n{e}")
        label_status.config(text="Not Connected", fg="red")


def validate_windows():
    """Validate timing window values (range and order)."""
    try:
        # Read numeric values from entry boxes
        values = [
            int(entries[k].get())
            for k in ["Perfect", "Good", "OK", "Poor"]
        ]
    except ValueError:
        messagebox.showerror("Invalid Input", "All windows must be integers.")
        return None

    # Ensure all values are within 1–500 ms
    if not all(1 <= v <= 500 for v in values):
        messagebox.showerror(
            "Range Error",
            "All values must be between 1 and 500 ms."
        )
        return None

    # Ensure increasing order: Perfect < Good < OK < Poor
    if not (values[0] < values[1] < values[2] < values[3]):
        messagebox.showerror(
            "Order Error",
            "Ensure Perfect < Good < OK < Poor."
        )
        return None

    return values


def write_to_device():
    """Send timing window configuration to device."""
    values = validate_windows()
    if values is None:
        return

    # Divide each window value by 2, but keep minimum as 1
    scaled = [max(1, v // 2) for v in values]

    # Re-check strict increasing order after scaling
    if not (scaled[0] < scaled[1] < scaled[2] < scaled[3]):
        # Optional: show before/after to help debugging
        before = f"{values[0]},{values[1]},{values[2]},{values[3]}"
        after = f"{scaled[0]},{scaled[1]},{scaled[2]},{scaled[3]}"
        messagebox.showerror(
            "Order Error After Scaling",
            "After dividing by 2, the windows must still satisfy:\n"
            "Perfect < Good < OK < Poor.\n\n"
            f"Before (ms): {before}\n"
            f"After (/2):  {after}"
        )
        return

    if serial_connection is None or not serial_connection.is_open:
        messagebox.showerror("Error", "Serial not connected.")
        return

    try:
        # Construct command string for standard and alternate formats
        cmd = f"SETWIN {scaled[0]} {scaled[1]} {scaled[2]} {scaled[3]}\n"
        serial_connection.write(cmd.encode())

        # Also send alternate format
        cmd_alt = f"SETWIN:{scaled[0]},{scaled[1]},{scaled[2]},{scaled[3]}\n"
        serial_connection.write(cmd_alt.encode())

        # Read echo (wait for MCU reply)
        response = serial_connection.readline().decode(errors="ignore").strip()
        if response:
            add_log(f"Device: {response}")  # Show on the right-side console
        else:
            add_log("[No Response]")  # No response received

        messagebox.showinfo("Success", f"Sent: {cmd.strip()}")
    except Exception as e:
        messagebox.showerror("Send Failed", str(e))


def read_from_device():
    """Read timing window configuration from device."""
    # Check if serial connection is valid before proceeding
    if serial_connection is None or not serial_connection.is_open:
        messagebox.showerror("Error", "Serial not connected.")
        return

    try:
        # Clear any existing data in the serial input buffer
        serial_connection.reset_input_buffer()
        # Send command to request timing window configuration
        serial_connection.write(b"GETWIN\n")
        # Read one line of response from device
        response = serial_connection.readline().decode().strip()
        print("Response from device:", response)

        # Parse valid response starting with keyword "WINDOW"
        if response.startswith("WINDOW"):
            parts = response.split()
            # Expecting: WINDOW <Perfect> <Good> <OK> <Poor>
            if len(parts) == 5:
                for i, key in enumerate(["Perfect", "Good", "OK", "Poor"]):
                    # Update GUI entries with received values
                    entries[key].delete(0, tk.END)
                    entries[key].insert(0, parts[i + 1])
                messagebox.showinfo("Read Success", "Timing windows loaded.")
                return

        # Warn user if unexpected data received
        messagebox.showwarning("Invalid Response", f"Unexpected: {response}")
    except Exception as e:
        # Catch all serial I/O or decoding errors
        messagebox.showerror("Read Failed", str(e))


def restore_defaults():
    """Restore default timing window values."""
    # Reset each timing entry box to its default (from DEFAULT_WINDOWS)
    for k, v in DEFAULT_WINDOWS.items():
        entries[k].delete(0, tk.END)
        entries[k].insert(0, str(v))


# ========================= Layout: Serial Connection =========================
# This frame manages serial port selection and connection controls
frame_serial = tk.LabelFrame(root, text="Serial Connection", padx=10, pady=10)
frame_serial.pack(fill="x", padx=10, pady=5)

# Dropdown box listing available COM ports
combo_serial = ttk.Combobox(frame_serial, state="readonly", width=20)
combo_serial.pack(side="left", padx=5)

# Refresh button to rescan serial ports
button_refresh = tk.Button(
    frame_serial,
    text="Refresh",
    command=refresh_serial_ports
)
button_refresh.pack(side="left", padx=5)

# Connect button to open selected serial port
button_connect = tk.Button(
    frame_serial,
    text="Connect",
    command=connect_serial
)
button_connect.pack(side="left", padx=5)

# Status label to show connection state (red = disconnected, green = connected)
label_status = tk.Label(frame_serial, text="Not Connected", fg="red")
label_status.pack(side="left", padx=10)


# ========================= Layout: Timing Windows =========================
# Frame for configuring scoring window thresholds
frame_timing = tk.LabelFrame(
    root,
    text="Scoring Timing Windows (ms)",
    padx=10,
    pady=10
)
frame_timing.pack(fill="x", padx=(10, 500), pady=5, anchor="w")

entries = {}
# Create labeled entry fields for each timing category
for idx, level in enumerate(["Perfect", "Good", "OK", "Poor"]):
    label = tk.Label(frame_timing, text=f"{level}:")
    label.grid(row=idx, column=0, sticky="e", padx=5, pady=5)
    entry = tk.Entry(frame_timing, width=10)
    entry.grid(row=idx, column=1, padx=5, pady=5)
    entry.insert(0, str(DEFAULT_WINDOWS[level]))  # Insert default values
    entries[level] = entry

frame_outer = tk.Frame(root, width=500, height=80)
frame_outer.pack(anchor="w")
frame_outer.pack_propagate(False)  # Prevent shrinking to content size

frame_buttons = tk.Frame(frame_outer)
frame_buttons.pack(expand=True)

# Action buttons for reading/writing/resetting timing windows
tk.Button(
    frame_buttons,
    text="Read from Device",
    command=read_from_device
).pack(
    side="left",
    padx=10
)
tk.Button(frame_buttons, text="Write to Device", command=write_to_device).pack(
    side="left", padx=10
)
tk.Button(
    frame_buttons,
    text="Restore Defaults",
    command=restore_defaults
).pack(
    side="left",
    padx=10
)


# ========================= microSD File Management =========================
# Variable to store the currently selected SD card path
sd_path = tk.StringVar()


def select_sd_folder():
    """Choose microSD folder path."""
    # Let user pick a directory as SD card root
    folder = filedialog.askdirectory(title="Select microSD Card Folder")
    if folder:
        sd_path.set(folder)
        refresh_sd_file_list()  # Refresh visible file list after selection


def refresh_sd_file_list():
    """Refresh the list of .tsq and .wav files."""
    # Clear current listbox content
    file_listbox.delete(0, tk.END)
    path = sd_path.get()

    # Skip if path not set or invalid
    if not os.path.exists(path):
        return

    # List all valid .tsq or .wav files in selected directory
    for fname in os.listdir(path):
        if fname.lower().endswith((".tsq", ".wav")):
            file_listbox.insert(tk.END, fname)


def add_file_to_sd():
    """Copy a file to the SD card folder."""
    # Open file dialog to select a .tsq or .wav file
    src_path = filedialog.askopenfilename(
        filetypes=[("tsq/wav", "*.tsq *.wav")]
    )
    if src_path:
        # Determine destination path inside selected SD directory
        dst_path = os.path.join(sd_path.get(), os.path.basename(src_path))
        try:
            shutil.copy(src_path, dst_path)  # Perform file copy
            refresh_sd_file_list()           # Update displayed file list
        except Exception as e:
            # Display error if copy fails (e.g., permission denied)
            messagebox.showerror("Copy Failed", f"Could not copy file:\n{e}")


def delete_selected_file():
    """Delete selected files from SD card."""
    selections = file_listbox.curselection()
    if not selections:
        messagebox.showwarning(
            "No Selection",
            "Please select files to delete."
        )
        return

    failed = []
    # Iterate backwards so listbox indices remain valid while deleting
    for index in selections[::-1]:
        filename = file_listbox.get(index)
        path = os.path.join(sd_path.get(), filename)
        try:
            os.remove(path)
        except Exception:
            failed.append(filename)  # Collect names that could not be removed

    refresh_sd_file_list()
    if failed:
        # Report which files could not be deleted
        messagebox.showerror(
            "Delete Failed",
            "Could not delete:\n" + "\n".join(failed)
        )


def reload_sd_files():
    """Manually refresh file list."""
    refresh_sd_file_list()
    messagebox.showinfo("Refreshed", "microSD file list refreshed.")


def update_drive_list():
    """List available removable drives."""
    drives = []
    # Scan all mounted partitions
    # and filter for removable or valid file systems
    for dp in psutil.disk_partitions():
        if not os.path.isdir(dp.mountpoint):
            continue
        if "removable" in dp.opts or dp.fstype != "":
            drives.append(dp.mountpoint)

    # Populate dropdown with detected drives
    drive_combo["values"] = drives
    if drives:
        drive_combo.current(0)       # Auto-select the first detected drive
        sd_path.set(drives[0])       # Update path variable
        refresh_sd_file_list()       # Display its contents immediately


# ========================= SD Layout =========================
# GUI section for managing SD card content (add, delete, refresh)
frame_sd = tk.LabelFrame(
    root,
    text="microSD File Management",
    padx=10,
    pady=10
)
frame_sd.pack(fill="x", padx=(10, 500), pady=5, anchor="w")

# --- Path selection row ---
frame_path = tk.Frame(frame_sd)
frame_path.pack(fill="x")

tk.Label(frame_path, text="SD Card Path:").pack(side="left")
tk.Entry(frame_path, textvariable=sd_path, width=40).pack(side="left", padx=5)

# Dropdown for detected drives
drive_combo = ttk.Combobox(frame_path, state="readonly", width=10)
drive_combo.pack(side="left", padx=5)
# Update current path and refresh file list when selection changes
drive_combo.bind(
    "<<ComboboxSelected>>",
    lambda e: sd_path.set(drive_combo.get()) or refresh_sd_file_list(),
)

# --- File list display ---
# Shows all .tsq / .wav files currently in the selected SD folder
file_listbox = tk.Listbox(frame_sd, height=6, selectmode=tk.EXTENDED)
file_listbox.pack(fill="both", expand=True, padx=10, pady=5)

# --- File operation buttons ---
frame_ops = tk.Frame(frame_sd)
frame_ops.pack()

tk.Button(
    frame_ops,
    text="Add File",
    command=add_file_to_sd
).pack(
    side="left",
    padx=10
)
tk.Button(frame_ops, text="Delete File", command=delete_selected_file).pack(
    side="left", padx=10
)
tk.Button(frame_ops, text="File Info", command=lambda: show_file_info()).pack(
    side="left", padx=10
)
tk.Button(
    frame_ops,
    text="Reload",
    command=reload_sd_files
).pack(
    side="left",
    padx=10
)

# Initialize by scanning available drives at startup
update_drive_list()


# ========================= File Info =========================
def show_file_info():
    """Display metadata of selected .tsq file."""
    # Ensure a file is selected from the listbox
    selection = file_listbox.curselection()
    if not selection:
        messagebox.showwarning(
            "No Selection",
            "Please select a file to inspect."
        )
        return

    filename = file_listbox.get(selection[0])
    path = os.path.join(sd_path.get(), filename)

    # Restrict info lookup to .tsq files only
    if not filename.lower().endswith(".tsq"):
        messagebox.showerror(
            "Invalid File",
            "Only .tsq files are supported for info."
        )
        return

    try:
        # Read the entire .tsq file into memory
        with open(path, "r") as f_in:
            lines = f_in.readlines()

        # Initialize placeholders for extracted metadata
        bpm = difficulty = name = artist = None

        # Parse header fields line by line
        for line in lines:
            line = line.strip()
            if line.startswith("BPM:"):
                bpm = line.replace("BPM:", "").strip()
            elif line.startswith("Difficulty:"):
                difficulty = line.replace("Difficulty:", "").strip()
            elif line.startswith("Name:"):
                name = line.replace("Name:", "").strip()
            elif line.startswith("Artist:"):
                artist = line.replace("Artist:", "").strip()

        # Estimate length from .wav if present
        duration_str = "(Unknown)"
        wav_path = os.path.join(
            sd_path.get(),
            os.path.splitext(filename)[0] + ".wav"
        )
        if os.path.exists(wav_path):
            try:
                with wave.open(wav_path, "r") as wav_file:
                    # Total number of audio frames
                    frames = wav_file.getnframes()
                    rate = wav_file.getframerate()    # Sampling rate (Hz)
                    seconds = round(frames / float(rate))
                    mm, ss = divmod(seconds, 60)      # Convert to MM:SS format
                    duration_str = f"{mm:02d}:{ss:02d}"
            except Exception:
                # Skip duration if .wav cannot be opened
                pass

        info = (
            f"File: {filename}\n\n"
            f"Name: {name or '(Unknown)'}\n"
            f"Artist: {artist or '(Unknown)'}\n"
            f"Difficulty: {difficulty or '(Unknown)'}\n"
            f"BPM: {bpm or '(Unknown)'}\n"
            f"Estimated Length: {duration_str}"
        )

        # Show metadata summary in popup
        messagebox.showinfo("Sequence Info", info)
    except Exception as e:
        # Handle parsing or file I/O errors
        messagebox.showerror("Error", f"Could not parse file:\n{e}")


# ========================= Blackboard Conversion =========================
# GUI section for converting Blackboard sample files (.csv / .tsq)
frame_convert = tk.LabelFrame(
    root,
    text="Blackboard Sample Conversion",
    padx=10,
    pady=10
)
frame_convert.pack(fill="x", padx=10, pady=5)

# Holds the currently selected Blackboard file path
bb_file_path = tk.StringVar()


def select_bb_file():
    """Select Blackboard file (.tsq or .csv)."""
    file_path = filedialog.askopenfilename(
        title="Select Blackboard Sample File",
        filetypes=[("TSQ/CSV Files", "*.tsq *.csv"), ("All Files", "*.*")],
    )
    if file_path:
        bb_file_path.set(file_path)  # Store selected path


def convert_bb_to_tsq():
    """Convert Blackboard file to .tsq and copy to SD."""
    # Ensure both file and SD path are selected before conversion
    if not bb_file_path.get():
        messagebox.showwarning(
            "No File",
            "Please select a Blackboard file first."
        )
        return

    if not sd_path.get():
        messagebox.showwarning(
            "No SD Path",
            "Please select an SD card folder first."
        )
        return

    try:
        # Define output filename (same base name, .tsq extension)
        tsq_name = (
                os.path.splitext(
                    os.path.basename(bb_file_path.get())
                )[0]
                + ".tsq"
        )
        tsq_path = os.path.join(sd_path.get(), tsq_name)

        # --- Case 1: Input file is already .tsq ---
        if bb_file_path.get().lower().endswith(".tsq"):
            shutil.copy(bb_file_path.get(), tsq_path)

        # --- Case 2: Convert from .csv ---
        elif bb_file_path.get().lower().endswith(".csv"):
            with open(bb_file_path.get(), "r") as f_in, open(
                    tsq_path, "w"
            ) as f_out:
                f_out.write("BPM: 120\nDifficulty: EASY\n---\n")
                # Write minimal .tsq header for compatibility
                for line in f_in:
                    parts = line.strip().split(",")
                    # Expect at least 2 columns (e.g., timestamp, note)
                    if len(parts) >= 2:
                        f_out.write(f"{parts[0]} {parts[1]}\n")
                f_out.write(";\n")  # End-of-sequence marker

        # --- Case 3: Unsupported file type ---
        else:
            messagebox.showerror(
                "Unsupported Format", "Only .tsq and .csv are supported."
            )
            return

        # Refresh file list to show newly added/converted file
        refresh_sd_file_list()
        messagebox.showinfo("Success", f"Converted and saved as {tsq_name}")
    except Exception as e:
        # Handle I/O or conversion-related errors
        messagebox.showerror("Conversion Failed", str(e))


# --- Layout for Blackboard conversion controls ---
frame_select = tk.Frame(frame_convert)
frame_select.pack(fill="x")

tk.Label(frame_select, text="Blackboard File:").pack(side="left")
tk.Entry(
    frame_select,
    textvariable=bb_file_path,
    width=30
).pack(
    side="left",
    padx=5
)
tk.Button(
    frame_select,
    text="Browse",
    command=select_bb_file
).pack(
    side="left"
)
# Conversion execution button
tk.Button(
    frame_convert,
    text="Convert & Write to SD",
    command=convert_bb_to_tsq
).pack(
    pady=5
)


# ======================== PowerShell / Serial Console ========================
# GUI section for serial command console and PowerShell integration
frame_terminal = tk.LabelFrame(
    root,
    text="Serial Port Console",
    padx=10,
    pady=10
)
frame_terminal.place(x=550, y=100, width=430, height=360)

# --- Scrollable log display area ---
scroll_frame = tk.Frame(frame_terminal)
scroll_frame.pack(fill="both", expand=True)

scrollbar = tk.Scrollbar(scroll_frame)
scrollbar.pack(side="right", fill="y")

# Listbox serves as the text console (black terminal-style)
console_listbox = tk.Listbox(
    scroll_frame,
    bg="black",
    fg="white",
    font=("Consolas", 10),
    selectmode=tk.EXTENDED,
    yscrollcommand=scrollbar.set,
)
console_listbox.pack(fill="both", expand=True, padx=5, pady=5)

# Link scrollbar movement to the listbox view
scrollbar.config(command=console_listbox.yview)


def add_log(message):
    """Append a line to the console listbox."""
    console_listbox.insert(tk.END, message)
    console_listbox.yview(tk.END)  # Auto-scroll to latest message


def run_serial_status():
    """List serial ports using pure Python (no PowerShell)."""
    # Clear the console display area
    console_listbox.delete(0, tk.END)

    try:
        # Get all available serial port information
        ports = list_ports.comports()

        if ports:
            # Display each serial port with its description
            for port in ports:
                add_log(f"{port.device} - {port.description}")
        else:
            add_log("[No serial ports found]")
    except Exception as e:
        # Show an error message if serial listing fails
        messagebox.showerror("Serial Listing Error", str(e))


def send_serial_command():
    """Send a command to the connected serial device and read one line."""
    # Ensure serial connection is active
    if serial_connection is None or not serial_connection.is_open:
        messagebox.showerror("Serial Error", "Serial not connected.")
        return

    cmd = command_entry.get().strip()
    if not cmd:
        return  # Skip if input box is empty

    try:
        # Write the command followed by newline to the serial device
        serial_connection.write((cmd + "\n").encode())

        # Wait for and read one line of response
        response = serial_connection.readline().decode(errors="ignore").strip()

        add_log(f"> {cmd}")  # Show sent command
        if response:
            add_log(response)  # Display device response if any

        command_entry.delete(0, tk.END)  # Clear input box
    except Exception as e:
        # Catch write/read/encoding errors
        messagebox.showerror("Send Failed", str(e))


def serial_listener():
    """
    Background thread:
    continuously read from serial and append to console.
    """
    while True:
        if serial_connection and serial_connection.is_open:
            try:
                # Check if new data is available on serial port
                if serial_connection.in_waiting:
                    line = (
                        serial_connection.readline()
                        .decode(errors="ignore")
                        .strip()
                    )
                    if line:
                        add_log(line)  # Append to console asynchronously
            except Exception:
                # Silently ignore transient serial errors
                pass


# --- User input and control buttons for the console ---
command_entry = tk.Entry(frame_terminal, width=40)
command_entry.pack(pady=5)
# Bind the Enter key to trigger command sending
command_entry.bind("<Return>", lambda event: send_serial_command())

frame_send = tk.Frame(frame_terminal)
frame_send.pack(fill="x", pady=(5, 0))

# Button to send manual serial command
tk.Button(frame_send, text="Send Command", command=send_serial_command).pack(
    side="right", padx=10
)

# Button to list all available serial ports using PowerShell
tk.Button(frame_send, text="List Serial", command=run_serial_status).pack(
    side="left", padx=10
)

# --- Start background listener thread for incoming serial data ---
listener_thread = threading.Thread(target=serial_listener, daemon=True)
listener_thread.start()

# Initial serial port scan
refresh_serial_ports()

# --- Enter main Tkinter event loop ---
root.mainloop()
