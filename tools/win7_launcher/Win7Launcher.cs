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
    private Label lblStatus;
    private string configFile = "horizon_win7_config.txt";

    [STAThread]
    public static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new Win7Launcher());
    }

    public Win7Launcher()
    {
        this.Text = "Horizon - Windows 7 Edition";
        this.Size = new Size(420, 280);
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
            Size = new Size(360, 25),
            Font = headerFont,
            ForeColor = Color.FromArgb(88, 101, 242)
        };
        this.Controls.Add(lblTitle);

        Label lblUser = new Label()
        {
            Text = "Player Name:",
            Location = new Point(20, 55),
            Size = new Size(100, 20),
            Font = regularFont
        };
        this.Controls.Add(lblUser);

        txtUsername = new TextBox()
        {
            Location = new Point(125, 52),
            Size = new Size(245, 23),
            Font = regularFont,
            Text = "Player"
        };
        this.Controls.Add(txtUsername);

        Label lblGfx = new Label()
        {
            Text = "Graphics API:",
            Location = new Point(20, 95),
            Size = new Size(100, 20),
            Font = regularFont
        };
        this.Controls.Add(lblGfx);

        rbOpenGL = new RadioButton()
        {
            Text = "OpenGL (Recommended for Win7)",
            Location = new Point(125, 93),
            Size = new Size(250, 22),
            Font = regularFont,
            Checked = true
        };
        this.Controls.Add(rbOpenGL);

        rbVulkan = new RadioButton()
        {
            Text = "Vulkan (If supported by your GPU driver)",
            Location = new Point(125, 120),
            Size = new Size(260, 22),
            Font = regularFont
        };
        this.Controls.Add(rbVulkan);

        btnPlay = new Button()
        {
            Text = "PLAY HORIZON",
            Location = new Point(20, 160),
            Size = new Size(360, 42),
            Font = new Font("Segoe UI", 11, FontStyle.Bold),
            BackColor = Color.FromArgb(46, 125, 50),
            ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat
        };
        btnPlay.FlatAppearance.BorderSize = 0;
        btnPlay.Click += BtnPlay_Click;
        this.Controls.Add(btnPlay);

        lblStatus = new Label()
        {
            Text = "Ready to start",
            Location = new Point(20, 215),
            Size = new Size(360, 20),
            Font = new Font("Segoe UI", 8, FontStyle.Italic),
            ForeColor = Color.Gray,
            TextAlign = ContentAlignment.MiddleCenter
        };
        this.Controls.Add(lblStatus);

        LoadConfig();
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
                "backend=" + backend
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

        string user = txtUsername.Text.Trim();
        if (string.IsNullOrEmpty(user)) user = "Player";

        ProcessStartInfo psi = new ProcessStartInfo();
        psi.FileName = clientExe;
        psi.WorkingDirectory = AppDomain.CurrentDomain.BaseDirectory;
        psi.Arguments = "--home . --resources .";

        if (rbOpenGL.Checked)
        {
            psi.EnvironmentVariables["WGPU_BACKEND"] = "gl";
        }
        else
        {
            psi.EnvironmentVariables["WGPU_BACKEND"] = "vulkan";
        }

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
