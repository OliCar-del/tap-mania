import tkinter as tk
from tkinter import ttk, messagebox
import serial.tools.list_ports

import os
import shutil
from tkinter import filedialog

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
root.geometry("500x400")

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
        label_status.config(text="✅ Connected", fg="green")
    except Exception as e:
        messagebox.showerror("Connection Failed", f"Unable to open port:\n{e}")
        label_status.config(text="❌ Not Connected", fg="red")


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


# Write current timing settings to device (placeholder)
def write_to_device():
    values = validate_windows()
    if values is None:
        return
    # Placeholder for sending via serial
    messagebox.showinfo("Write Success", f"Settings sent: {values}")


# Read timing settings from device (placeholder)
def read_from_device():
    # Placeholder: should be replaced with real serial read
    for k, v in DEFAULT_WINDOWS.items():
        entries[k].delete(0, tk.END)
        entries[k].insert(0, str(v))
    messagebox.showinfo("Read Complete", "Device settings loaded (example).")


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
label_status = tk.Label(frame_serial, text="❌ Not Connected", fg="red")
label_status.pack(side="left", padx=10)

# Layout: Timing window inputs
frame_timing = tk.LabelFrame(root, text="Scoring Timing Windows (ms)", padx=10, pady=10)
frame_timing.pack(fill="x", padx=10, pady=5)

entries = {}
for idx, level in enumerate(["Perfect", "Good", "OK", "Poor"]):
    label = tk.Label(frame_timing, text=f"{level}:")
    label.grid(row=idx, column=0, sticky="e", padx=5, pady=5)
    entry = tk.Entry(frame_timing, width=10)
    entry.grid(row=idx, column=1, padx=5, pady=5)
    entry.insert(0, str(DEFAULT_WINDOWS[level]))
    entries[level] = entry

# Operation buttons
frame_buttons = tk.Frame(root)
frame_buttons.pack(pady=10)

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
frame_sd.pack(fill="both", expand=True, padx=10, pady=5)

# Path input and folder selection
frame_path = tk.Frame(frame_sd)
frame_path.pack(fill="x")
tk.Label(frame_path, text="SD Card Path:").pack(side="left")
tk.Entry(frame_path, textvariable=sd_path, width=40).pack(side="left", padx=5)
tk.Button(frame_path, text="Browse", command=select_sd_folder).pack(side="left")

# File list display
file_listbox = tk.Listbox(frame_sd, height=6)
file_listbox.pack(fill="both", expand=True, padx=10, pady=5)

# Add/Delete buttons
frame_ops = tk.Frame(frame_sd)
frame_ops.pack()
tk.Button(frame_ops, text="Add File", command=add_file_to_sd).pack(side="left", padx=10)
tk.Button(frame_ops, text="Delete File", command=delete_selected_file).pack(side="left", padx=10)

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

# Initial serial port scan
refresh_serial_ports()
root.mainloop()

