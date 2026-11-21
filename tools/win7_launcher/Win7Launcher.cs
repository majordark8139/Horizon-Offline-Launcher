using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

public class Win7Launcher : Form
{
    private TextBox txtUsername;
    private RadioButton rbOpenGL;
    private RadioButton rbVulkan;
    private Button btnPlay;
    private Button btnPrepare;
    private Label lblStatus;
    private Label lblMinecraftPath;
    private string configFile = "horizon_win7_config.txt";
    private string selectedMinecraftDir = "";

    [STAThread]
    public static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new Win7Launcher());
    }

    public Win7Launcher()
    {
        this.Text = "Horizon Offline (Windows 7 Edition)";
        this.Size = new Size(460, 340);
        this.FormBorderStyle = FormBorderStyle.FixedDialog;
        this.MaximizeBox = false;
        this.StartPosition = FormStartPosition.CenterScreen;
        this.BackColor = Color.FromArgb(32, 34, 37);
        this.ForeColor = Color.White;

        Font headerFont = new Font("Segoe UI", 12, FontStyle.Bold);
        Font regularFont = new Font("Segoe UI", 9, FontStyle.Regular);

        Label lblTitle = new Label()
        {
            Text = "Horizon Offline (Windows 7)",
            Location = new Point(20, 15),
            Size = new Size(400, 25),
            Font = headerFont,
            ForeColor = Color.FromArgb(88, 101, 242)
        };
        this.Controls.Add(lblTitle);

        Label lblUser = new Label()
        {
            Text = "Player Name:",
            Location = new Point(20, 55),
            Size = new Size(110, 20),
            Font = regularFont
        };
        this.Controls.Add(lblUser);

        txtUsername = new TextBox()
        {
            Location = new Point(135, 52),
            Size = new Size(285, 23),
            Font = regularFont,
            Text = "Player"
        };
        this.Controls.Add(txtUsername);

        Label lblGfx = new Label()
        {
            Text = "Graphics Mode:",
            Location = new Point(20, 90),
            Size = new Size(110, 20),
            Font = regularFont
        };
        this.Controls.Add(lblGfx);

        rbOpenGL = new RadioButton()
        {
            Text = "OpenGL (Recommended for Windows 7)",
            Location = new Point(135, 88),
            Size = new Size(285, 22),
            Font = regularFont,
            Checked = true
        };
        this.Controls.Add(rbOpenGL);

        rbVulkan = new RadioButton()
        {
            Text = "Vulkan (Only if your GPU driver supports it)",
            Location = new Point(135, 112),
            Size = new Size(285, 22),
            Font = regularFont
        };
        this.Controls.Add(rbVulkan);

        lblMinecraftPath = new Label()
        {
            Text = "Minecraft Assets: Checking...",
            Location = new Point(20, 145),
            Size = new Size(310, 30),
            Font = new Font("Segoe UI", 8, FontStyle.Regular),
            ForeColor = Color.LightGray
        };
        this.Controls.Add(lblMinecraftPath);

        btnPrepare = new Button()
        {
            Text = "Browse...",
            Location = new Point(340, 143),
            Size = new Size(80, 28),
            Font = regularFont,
            BackColor = Color.FromArgb(64, 68, 75),
            ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat
        };
        btnPrepare.FlatAppearance.BorderSize = 0;
        btnPrepare.Click += BtnBrowse_Click;
        this.Controls.Add(btnPrepare);

        btnPlay = new Button()
        {
            Text = "PLAY HORIZON",
            Location = new Point(20, 190),
            Size = new Size(400, 48),
            Font = new Font("Segoe UI", 12, FontStyle.Bold),
            BackColor = Color.FromArgb(46, 125, 50),
            ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat
        };
        btnPlay.FlatAppearance.BorderSize = 0;
        btnPlay.Click += BtnPlay_Click;
        this.Controls.Add(btnPlay);

        lblStatus = new Label()
        {
            Text = "Checking assets...",
            Location = new Point(20, 255),
            Size = new Size(400, 25),
            Font = new Font("Segoe UI", 9, FontStyle.Italic),
            ForeColor = Color.Gray,
            TextAlign = ContentAlignment.MiddleCenter
        };
        this.Controls.Add(lblStatus);

        LoadConfig();
        AutoDetectAssets();
    }

    private string FindDefaultMinecraftDir()
    {
        string appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        string[] candidates = new string[]
        {
            Path.Combine(appData, ".minecraft"),
            Path.Combine(appData, "PrismLauncher"),
            Path.Combine(appData, "MultiMC"),
            Path.Combine(appData, "ModrinthApp"),
            Path.Combine(appData, "ATLauncher"),
            Path.Combine(appData, ".tlauncher")
        };

        foreach (string path in candidates)
        {
            if (Directory.Exists(path)) return path;
        }
        return "";
    }

    private bool AreResourcesReady()
    {
        string baseDir = AppDomain.CurrentDomain.BaseDirectory;
        if (File.Exists(Path.Combine(baseDir, "horizon-resources.json"))) return true;
        if (File.Exists(Path.Combine(baseDir, "resources", "horizon-resources.json"))) return true;
        if (File.Exists(Path.Combine(baseDir, "assets", "minecraft", "sounds.json"))) return true;
        if (Directory.Exists(Path.Combine(baseDir, "resources"))) return true;
        return false;
    }

    private void AutoDetectAssets()
    {
        if (AreResourcesReady())
        {
            lblMinecraftPath.Text = "Game Assets: Ready to play";
            lblMinecraftPath.ForeColor = Color.LightGreen;
            lblStatus.Text = "Ready to start";
            return;
        }

        if (string.IsNullOrEmpty(selectedMinecraftDir) || !Directory.Exists(selectedMinecraftDir))
        {
            selectedMinecraftDir = FindDefaultMinecraftDir();
        }

        if (!string.IsNullOrEmpty(selectedMinecraftDir))
        {
            lblMinecraftPath.Text = "Found Minecraft: " + Path.GetFileName(selectedMinecraftDir);
            lblMinecraftPath.ForeColor = Color.Orange;
            lblStatus.Text = "First launch: Click PLAY to auto-prepare assets";
        }
        else
        {
            lblMinecraftPath.Text = "Assets missing! Click Browse to locate .minecraft";
            lblMinecraftPath.ForeColor = Color.LightCoral;
            lblStatus.Text = "Minecraft folder required for initial setup";
        }
    }

    private void BtnBrowse_Click(object sender, EventArgs e)
    {
        using (FolderBrowserDialog fbd = new FolderBrowserDialog())
        {
            fbd.Description = "Select your .minecraft folder (or Prism/TLauncher folder)";
            if (fbd.ShowDialog() == DialogResult.OK)
            {
                selectedMinecraftDir = fbd.SelectedPath;
                AutoDetectAssets();
                SaveConfig();
            }
        }
    }

    private bool RunPrepare(string mcDir)
    {
        string prepareExe = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "horizon-prepare.exe");
        if (!File.Exists(prepareExe))
        {
            return false;
        }

        lblStatus.Text = "Preparing game assets from Minecraft (one-time setup)...";
        lblStatus.ForeColor = Color.Yellow;
        Application.DoEvents();

        ProcessStartInfo psi = new ProcessStartInfo();
        psi.FileName = prepareExe;
        psi.Arguments = "--prepare --minecraft-dir \"" + mcDir + "\" --output .";
        psi.WorkingDirectory = AppDomain.CurrentDomain.BaseDirectory;
        psi.WindowStyle = ProcessWindowStyle.Hidden;
        psi.CreateNoWindow = true;
        psi.UseShellExecute = false;

        try
        {
            using (Process p = Process.Start(psi))
            {
                p.WaitForExit();
                return p.ExitCode == 0 || AreResourcesReady();
            }
        }
        catch
        {
            return false;
        }
    }

    private void LoadConfig()
    {
        try
        {
            if (File.Exists(configFile))
            {
                string[] lines = File.ReadAllLines(configFile);
                foreach (string line in lines)
                {
                    if (line.StartsWith("username=")) txtUsername.Text = line.Substring(9).Trim();
                    if (line.StartsWith("backend=vulkan")) { rbVulkan.Checked = true; rbOpenGL.Checked = false; }
                    if (line.StartsWith("backend=gl")) { rbOpenGL.Checked = true; rbVulkan.Checked = false; }
                    if (line.StartsWith("mcdir=")) selectedMinecraftDir = line.Substring(6).Trim();
                }
            }
        }
        catch { }
    }

    private void SaveConfig()
    {
        try
        {
            string backend = rbVulkan.Checked ? "vulkan" : "gl";
            File.WriteAllLines(configFile, new string[] {
                "username=" + txtUsername.Text.Trim(),
                "backend=" + backend,
                "mcdir=" + selectedMinecraftDir
            });
        }
        catch { }
    }

    private void BtnPlay_Click(object sender, EventArgs e)
    {
        string clientExe = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "horizon-client.exe");
        if (!File.Exists(clientExe))
        {
            MessageBox.Show("horizon-client.exe not found in this folder!\nPlease place the launcher next to the game files.", "File Missing", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        SaveConfig();

        if (!AreResourcesReady())
        {
            if (string.IsNullOrEmpty(selectedMinecraftDir) || !Directory.Exists(selectedMinecraftDir))
            {
                selectedMinecraftDir = FindDefaultMinecraftDir();
            }

            if (string.IsNullOrEmpty(selectedMinecraftDir))
            {
                MessageBox.Show("First launch setup: Please select your Minecraft folder to extract game textures and sounds.", "Assets Needed", MessageBoxButtons.OK, MessageBoxIcon.Information);
                BtnBrowse_Click(sender, e);
                if (string.IsNullOrEmpty(selectedMinecraftDir)) return;
            }

            btnPlay.Enabled = false;
            bool ok = RunPrepare(selectedMinecraftDir);
            btnPlay.Enabled = true;

            if (!ok && !AreResourcesReady())
            {
                MessageBox.Show("Could not extract assets from the selected folder.\nPlease ensure it contains a valid Minecraft 26.3 installation.", "Preparation Failed", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                lblStatus.Text = "Preparation failed";
                lblStatus.ForeColor = Color.Red;
                return;
            }
        }

        string user = txtUsername.Text.Trim();
        if (string.IsNullOrEmpty(user)) user = "Player";

        ProcessStartInfo psi = new ProcessStartInfo();
        psi.FileName = clientExe;
        psi.WorkingDirectory = AppDomain.CurrentDomain.BaseDirectory;
        string resPath = ".";
        if (File.Exists(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "resources", "horizon-resources.json")))
        {
            resPath = "resources";
        }
        psi.Arguments = "--home . --resources " + resPath;

        if (rbOpenGL.Checked)
        {
            psi.EnvironmentVariables["WGPU_BACKEND"] = "gl";
        }
        else
        {
            psi.EnvironmentVariables["WGPU_BACKEND"] = "vulkan";
        }

        psi.EnvironmentVariables["WGPU_VALIDATION"] = "0";
        psi.EnvironmentVariables["WGPU_DEBUG"] = "0";
        psi.EnvironmentVariables["WGPU_DISCARD_HAL_LABELS"] = "1";
        psi.EnvironmentVariables["MIMALLOC_PURGE_DELAY"] = "100";
        psi.EnvironmentVariables["MIMALLOC_EAGER_COMMIT"] = "0";
        psi.EnvironmentVariables["HORIZON_OFFLINE_NAME"] = user;
        psi.EnvironmentVariables["HORIZON_TELEMETRY_URL"] = "off";
        psi.UseShellExecute = false;

        try
        {
            lblStatus.Text = "Launching game...";
            lblStatus.ForeColor = Color.LightGreen;
            Process.Start(psi);
            this.WindowState = FormWindowState.Minimized;
        }
        catch (Exception ex)
        {
            MessageBox.Show("Failed to launch Horizon: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            lblStatus.Text = "Launch failed";
            lblStatus.ForeColor = Color.Red;
        }
    }
}
