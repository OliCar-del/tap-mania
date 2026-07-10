import tkinter as tk
from tkinter import ttk, messagebox
import serial.tools.list_ports

# Powershell
import subprocess

import os
import shutil
from tkinter import filedialog

import psutil #run pip install psutil

import wave

import threading

# Default timing window values (in ms)
DEFAULT_WINDOWS = {
    "Perfect": 100,
    "Good": 150,
    "OK": 200,
    "Poor": 250
}

# Initialize main window
root = tk.Tk()
root.title("tpmania Configuration Tool")
root.geometry("1000x800")

# Global variable for serial connection
serial_connection = None


# Scan available serial ports
def refresh_serial_ports():
    ports = serial.tools.list_ports.comports()
    port_list = [port.device for port in ports]
    combo_serial['values'] = port_list
    if port_list:
        combo_serial.current(0)


# Connect to selected serial port
def connect_serial():
    global serial_connection
    port = combo_serial.get()
    try:
        serial_connection = serial.Serial(port, 115200, timeout=1)
        label_status.config(text="Connected", fg="green")
    except Exception as e:
        messagebox.showerror("Connection Failed", f"Unable to open port:\n{e}")
        label_status.config(text="Not Connected", fg="red")


# Validate timing window values (range and order)
def validate_windows():
    try:
        values = [int(entries[k].get()) for k in ["Perfect", "Good", "OK", "Poor"]]
    except ValueError:
        messagebox.showerror("Invalid Input", "All windows must be integers.")
        return None
    if not all(1 <= v <= 500 for v in values):
        messagebox.showerror("Range Error", "All values must be between 1 and 500 ms.")
        return None
    if not (values[0] < values[1] < values[2] < values[3]):
        messagebox.showerror("Order Error", "Ensure Perfect < Good < OK < Poor.")
        return None
    return values


def write_to_device():
    values = validate_windows()
    if values is None:
        return
    if serial_connection is None or not serial_connection.is_open:
        messagebox.showerror("Error", "Serial not connected.")
        return
    try:
        # Construct and send command
        cmd = f"SET_WINDOW {values[0]} {values[1]} {values[2]} {values[3]}\n"
        serial_connection.write(cmd.encode())
        ##########################################
        # Also send new format: SETWIN:P,G,O,Po
        cmd_alt = f"SETWIN:{values[0]},{values[1]},{values[2]},{values[3]}\n"
        serial_connection.write(cmd_alt.encode())

        messagebox.showinfo("Success", f"Sent: {cmd.strip()}")
    except Exception as e:
        messagebox.showerror("Send Failed", str(e))

def read_from_device():
    if serial_connection is None or not serial_connection.is_open:
        messagebox.showerror("Error", "Serial not connected.")
        return
    try:
        serial_connection.reset_input_buffer()
        serial_connection.write(b"GET_WINDOW\n")
        response = serial_connection.readline().decode().strip()
        print("Response from device:", response)

        if response.startswith("WINDOW"):
            parts = response.split()
            if len(parts) == 5:
                for i, key in enumerate(["Perfect", "Good", "OK", "Poor"]):
                    entries[key].delete(0, tk.END)
                    entries[key].insert(0, parts[i + 1])
                messagebox.showinfo("Read Success", "Timing windows loaded.")
                return
        messagebox.showwarning("Invalid Response", f"Unexpected: {response}")
    except Exception as e:
        messagebox.showerror("Read Failed", str(e))


# Restore default values
def restore_defaults():
    for k, v in DEFAULT_WINDOWS.items():
        entries[k].delete(0, tk.END)
        entries[k].insert(0, str(v))


# Layout: Serial Connection
frame_serial = tk.LabelFrame(root, text="Serial Connection", padx=10, pady=10)
frame_serial.pack(fill="x", padx=10, pady=5)

combo_serial = ttk.Combobox(frame_serial, state="readonly", width=20)
combo_serial.pack(side="left", padx=5)
button_refresh = tk.Button(frame_serial, text="Refresh", command=refresh_serial_ports)
button_refresh.pack(side="left", padx=5)
button_connect = tk.Button(frame_serial, text="Connect", command=connect_serial)
button_connect.pack(side="left", padx=5)
label_status = tk.Label(frame_serial, text="Not Connected", fg="red")
label_status.pack(side="left", padx=10)

# Layout: Timing window inputs
frame_timing = tk.LabelFrame(root, text="Scoring Timing Windows (ms)", padx=10, pady=10)
frame_timing.pack(fill="x", padx=(10, 500), pady=5, anchor="w")


entries = {}
for idx, level in enumerate(["Perfect", "Good", "OK", "Poor"]):
    label = tk.Label(frame_timing, text=f"{level}:")
    label.grid(row=idx, column=0, sticky="e", padx=5, pady=5)
    entry = tk.Entry(frame_timing, width=10)
    entry.grid(row=idx, column=1, padx=5, pady=5)
    entry.insert(0, str(DEFAULT_WINDOWS[level]))
    entries[level] = entry

# # Operation buttons
# # frame_buttons = tk.Frame(root)
# frame_buttons = tk.Frame(root, width=500)
# # frame_buttons.pack(pady=10)
# frame_buttons.pack(pady=10, anchor="w")
#
#
# tk.Button(frame_buttons, text="Read from Device", command=read_from_device).pack(side="left", padx=10)
# tk.Button(frame_buttons, text="Write to Device", command=write_to_device).pack(side="left", padx=10)
# tk.Button(frame_buttons, text="Restore Defaults", command=restore_defaults).pack(side="left", padx=10)

frame_outer = tk.Frame(root, width=500, height=80)
frame_outer.pack(anchor="w")
frame_outer.pack_propagate(False)


frame_buttons = tk.Frame(frame_outer)
frame_buttons.pack(expand=True)


# tk.Button(frame_buttons, text="Read from Device").pack(side="left", padx=10)
# tk.Button(frame_buttons, text="Write to Device").pack(side="left", padx=10)
# tk.Button(frame_buttons, text="Restore Defaults").pack(side="left", padx=10)
tk.Button(frame_buttons, text="Read from Device", command=read_from_device).pack(side="left", padx=10)
tk.Button(frame_buttons, text="Write to Device", command=write_to_device).pack(side="left", padx=10)
tk.Button(frame_buttons, text="Restore Defaults", command=restore_defaults).pack(side="left", padx=10)



# microSD card path variable
sd_path = tk.StringVar()

# Choose microSD folder path
def select_sd_folder():
    folder = filedialog.askdirectory(title="Select microSD Card Folder")
    if folder:
        sd_path.set(folder)
        refresh_sd_file_list()

# Refresh file list (.tsq and .wav)
def refresh_sd_file_list():
    file_listbox.delete(0, tk.END)
    path = sd_path.get()
    if not os.path.exists(path): return
    for f in os.listdir(path):
        if f.lower().endswith(".tsq") or f.lower().endswith(".wav"):
            file_listbox.insert(tk.END, f)

# Copy a file to SD card folder
def add_file_to_sd():
    src_path = filedialog.askopenfilename(filetypes=[("tsq/wav", "*.tsq *.wav")])
    if src_path:
        dst_path = os.path.join(sd_path.get(), os.path.basename(src_path))
        try:
            shutil.copy(src_path, dst_path)
            refresh_sd_file_list()
        except Exception as e:
            messagebox.showerror("Copy Failed", f"Could not copy file:\n{e}")

# Delete selected files from SD card
def delete_selected_file():
    selection = file_listbox.curselection()
    if not selection:
        messagebox.showwarning("No Selection", "Please select a file to delete.")
        return
    filename = file_listbox.get(selection[0])
    path = os.path.join(sd_path.get(), filename)
    try:
        os.remove(path)
        refresh_sd_file_list()
    except Exception as e:
        messagebox.showerror("Delete Failed", f"Could not delete file:\n{e}")

# Layout: SD card management
frame_sd = tk.LabelFrame(root, text="microSD File Management", padx=10, pady=10)
frame_sd.pack(fill="x", padx=(10, 500), pady=5, anchor="w")

# Path input and folder selection
frame_path = tk.Frame(frame_sd)
frame_path.pack(fill="x")
tk.Label(frame_path, text="SD Card Path:").pack(side="left")
tk.Entry(frame_path, textvariable=sd_path, width=40).pack(side="left", padx=5)

drive_combo = ttk.Combobox(frame_path, state="readonly", width=10)
drive_combo.pack(side="left", padx=5)

def update_drive_list():
    drives = []
    for dp in psutil.disk_partitions():
        # Skip if the mount point is not a real directory
        if not os.path.isdir(dp.mountpoint):
            continue
        # Accept removable drives (Windows) or any drive with valid file system
        if 'removable' in dp.opts or dp.fstype != '':
            drives.append(dp.mountpoint)  # Use mountpoint instead of device

    # Update dropdown list of drives
    drive_combo['values'] = drives
    if drives:
        drive_combo.current(0)           # Select the first available drive
        sd_path.set(drives[0])           # Set it as the current SD path
        refresh_sd_file_list()           # Refresh the displayed file list

drive_combo.bind("<<ComboboxSelected>>", lambda e: sd_path.set(drive_combo.get()) or refresh_sd_file_list())


# show the details of the file ()
def show_file_info():
    selection = file_listbox.curselection()
    if not selection:
        messagebox.showwarning("No Selection", "Please select a file to inspect.")
        return

    filename = file_listbox.get(selection[0])
    path = os.path.join(sd_path.get(), filename)

    # Only support .tsq files
    if not filename.lower().endswith(".tsq"):
        messagebox.showerror("Invalid File", "Only .tsq files are supported for info.")
        return

    try:
        with open(path, "r") as f:
            lines = f.readlines()

        # Parse metadata
        bpm = None
        difficulty = None
        name = None
        artist = None
        note_lines = []

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
            elif line and not line.startswith("---") and not line.startswith(";"):
                if len(line.split()) == 2:
                    note_lines.append(line)

        # Estimate audio length by reading the corresponding .wav file
        duration_str = "(Unknown)"
        tsq_base = os.path.splitext(filename)[0]
        wav_path = os.path.join(sd_path.get(), tsq_base + ".wav")

        if os.path.exists(wav_path):
            try:
                with wave.open(wav_path, 'r') as wav_file:
                    frames = wav_file.getnframes()
                    rate = wav_file.getframerate()
                    seconds = round(frames / float(rate))
                    mm = seconds // 60
                    ss = seconds % 60
                    duration_str = f"{mm:02d}:{ss:02d}"
            except Exception as e:
                print(f"Failed to read .wav file: {e}")

                # Show info
                info = f"File: {filename}\n\n"
                info += f"Name: {name or '(Unknown)'}\n"
                info += f"Artist: {artist or '(Unknown)'}\n"
                info += f"Difficulty: {difficulty or '(Unknown)'}\n"
                info += f"BPM: {bpm or '(Unknown)'}\n"
                info += f"Estimated Length: {duration_str}"
                messagebox.showinfo("Sequence Info", info)

    except Exception as e:
        messagebox.showerror("Error", f"Could not parse file:\n{e}")


# File list display
file_listbox = tk.Listbox(frame_sd, height=6)
file_listbox.pack(fill="both", expand=True, padx=10, pady=5)

update_drive_list()
# Add/Delete buttons
frame_ops = tk.Frame(frame_sd)
frame_ops.pack()
tk.Button(frame_ops, text="Add File", command=add_file_to_sd).pack(side="left", padx=10)
tk.Button(frame_ops, text="Delete File", command=delete_selected_file).pack(side="left", padx=10)
tk.Button(frame_ops, text="File Info", command=show_file_info).pack(side="left", padx=10) # new button show file info details
# Right-click menu + multi-select + refresh button

def refresh_sd_context_menu(event):
    # Right-click context menu
    context_menu = tk.Menu(file_listbox, tearoff=0)
    context_menu.add_command(label="Delete Selected File(s)", command=delete_selected_file)
    context_menu.post(event.x_root, event.y_root)

def delete_selected_file():
    selections = file_listbox.curselection()
    if not selections:
        messagebox.showwarning("No Selection", "Please select files to delete.")
        return
    failed = []
    for index in selections[::-1]:  # Delete in reverse order
        filename = file_listbox.get(index)
        path = os.path.join(sd_path.get(), filename)
        try:
            os.remove(path)
        except Exception as e:
            failed.append(filename)
    refresh_sd_file_list()
    if failed:
        messagebox.showerror("Delete Failed", f"Could not delete:\n" + "\n".join(failed))

# Reload button
def reload_sd_files():
    refresh_sd_file_list()
    messagebox.showinfo("Refreshed", "microSD file list refreshed.")

# Add reload button
tk.Button(frame_ops, text="Reload", command=reload_sd_files).pack(side="left", padx=10)

# Enable multi-selection + right-click
file_listbox.config(selectmode=tk.EXTENDED)
file_listbox.bind("<Button-3>", refresh_sd_context_menu)

# Blackboard sample conversion
frame_convert = tk.LabelFrame(root, text="Blackboard Sample Conversion", padx=10, pady=10)
frame_convert.pack(fill="x", padx=10, pady=5)

bb_file_path = tk.StringVar()

# Select Blackboard file
def select_bb_file():
    file = filedialog.askopenfilename(title="Select Blackboard Sample File",
                                      filetypes=[("TSQ/CSV Files", "*.tsq *.csv"), ("All Files", "*.*")])
    if file:
        bb_file_path.set(file)

# Convert or copy Blackboard file to microSD
def convert_bb_to_tsq():
    if not bb_file_path.get():
        messagebox.showwarning("No File", "Please select a Blackboard file first.")
        return
    if not sd_path.get():
        messagebox.showwarning("No SD Path", "Please select an SD card folder first.")
        return
    try:
        # Output file name (same as original but with .tsq extension)
        tsq_name = os.path.splitext(os.path.basename(bb_file_path.get()))[0] + ".tsq"
        tsq_path = os.path.join(sd_path.get(), tsq_name)

        # Detect file type
        if bb_file_path.get().lower().endswith(".tsq"):
            # Directly copy Blackboard tsq file
            with open(bb_file_path.get(), "r") as f_in, open(tsq_path, "w") as f_out:
                for line in f_in:
                    f_out.write(line)
        elif bb_file_path.get().lower().endswith(".csv"):
            # Simple CSV-to-TSQ conversion logic (assuming each line is "time,button")
            with open(bb_file_path.get(), "r") as f_in, open(tsq_path, "w") as f_out:
                f_out.write("BPM: 120\nDifficulty: EASY\n---\n")  # Example metadata, can be adjusted as needed
                for line in f_in:
                    parts = line.strip().split(",")
                    if len(parts) >= 2:
                        time_ms, button = parts[0], parts[1]
                        f_out.write(f"{time_ms} {button}\n")
                f_out.write(";\n")  # End of file marker
        else:
            messagebox.showerror("Unsupported Format", "Only .tsq and .csv files are supported.")
            return

        refresh_sd_file_list()
        messagebox.showinfo("Success", f"Converted and saved as {tsq_name}")
    except Exception as e:
        messagebox.showerror("Conversion Failed", str(e))



# GUI layout
frame_select = tk.Frame(frame_convert)
frame_select.pack(fill="x")
tk.Label(frame_select, text="Blackboard File:").pack(side="left")
tk.Entry(frame_select, textvariable=bb_file_path, width=30).pack(side="left", padx=5)
tk.Button(frame_select, text="Browse", command=select_bb_file).pack(side="left")

tk.Button(frame_convert, text="Convert & Write to SD", command=convert_bb_to_tsq).pack(pady=5)

from tkinter.scrolledtext import ScrolledText

# Frame for PowerShell terminal (right side)
frame_terminal = tk.LabelFrame(root, text="Serial Port Console (PowerShell)", padx=10, pady=10)
frame_terminal.place(x=550, y=100, width=430, height=360)  # Right-side layout: adaptive to the right space

# Text box for output (readonly)
console_output = ScrolledText(frame_terminal, height=40, width=60, bg="black", fg="white", state="disabled")
console_output.pack(fill="both", expand=True, padx=5, pady=5)

# Run PowerShell command
def run_serial_status():
    try:
        # Run a simple PowerShell command to get serial ports
        result = subprocess.run(
            ["powershell", "-Command", "Get-CimInstance Win32_SerialPort | Select-Object Name, DeviceID"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )

        console_output.config(state="normal")
        console_output.delete("1.0", tk.END)  # Clear previous content
        if result.stdout:
            console_output.insert(tk.END, result.stdout)
        if result.stderr:
            console_output.insert(tk.END, "\n[ERROR]\n" + result.stderr)
        console_output.config(state="disabled")
    except Exception as e:
        messagebox.showerror("PowerShell Error", str(e))

# Entry for serial command
command_entry = tk.Entry(frame_terminal, width=40)
command_entry.pack(pady=5)

# Bind Enter key to trigger command send
command_entry.bind("<Return>", lambda event: send_serial_command())

# Send serial command
def send_serial_command():
    if serial_connection is None or not serial_connection.is_open:
        messagebox.showerror("Serial Error", "Serial not connected.")
        return
    cmd = command_entry.get().strip()
    if not cmd:
        return
    try:
        serial_connection.write((cmd + "\n").encode())  # Send with newline
        response = serial_connection.readline().decode(errors='ignore').strip()

        # Show response in console
        console_output.config(state="normal")
        console_output.insert(tk.END, f"> {cmd}\n")
        console_output.insert(tk.END, response + "\n")
        console_output.see(tk.END)
        console_output.config(state="disabled")

        command_entry.delete(0, tk.END)
    except Exception as e:
        messagebox.showerror("Send Failed", str(e))


# Background thread to read serial responses continuously
def serial_listener():
    while True:
        if serial_connection and serial_connection.is_open:
            try:
                if serial_connection.in_waiting:
                    line = serial_connection.readline().decode(errors='ignore').strip()
                    if line:
                        console_output.config(state="normal")
                        console_output.insert(tk.END, line + "\n")
                        console_output.see(tk.END)
                        console_output.config(state="disabled")
            except:
                pass


# Start background serial listener thread
listener_thread = threading.Thread(target=serial_listener, daemon=True)
listener_thread.start()


# Send Button
frame_send = tk.Frame(frame_terminal)
frame_send.pack(fill="x", pady=(5, 0))

tk.Button(frame_send, text="Send Command", command=send_serial_command).pack(side="right", padx=10)

# Initial serial port scan
refresh_serial_ports()
root.mainloop()
