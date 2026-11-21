using System.IO;
using System.Windows;

namespace RiseHalfMoon;

public sealed record AppOptions(string DataFolder, bool StrictOffline, int? AutoPlay);

/// <summary>
/// Argumentos opcionais:
///   --data &lt;pasta&gt;   pasta dos jogos (por omissão: "Jogos" ao lado do executável)
///   --download        descarrega todos os jogos sem abrir a janela e termina (registo em download.log)
///   --offline         nunca usa a internet, nem para completar ficheiros em falta
///   --play &lt;1-8&gt;     abre logo esse jogo
/// </summary>
public partial class App : Application
{
    private async void OnStartup(object sender, StartupEventArgs e)
    {
        var args = e.Args;
        string data = Path.Combine(AppContext.BaseDirectory, "Jogos");
        bool download = false, offline = false;
        int? play = null;

        for (int i = 0; i < args.Length; i++)
        {
            switch (args[i].ToLowerInvariant())
            {
                case "--data" when i + 1 < args.Length: data = args[++i]; break;
                case "--download": download = true; break;
                case "--offline": offline = true; break;
                case "--play" when i + 1 < args.Length && int.TryParse(args[i + 1], out var n): play = n; i++; break;
            }
        }

        var options = new AppOptions(Path.GetFullPath(data), offline, play);

        if (download)
        {
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            Shutdown(await RunHeadlessDownloadAsync(options));
            return;
        }

        var window = new MainWindow(options);
        MainWindow = window;
        window.Show();
    }

    private static async Task<int> RunHeadlessDownloadAsync(AppOptions options)
    {
        Directory.CreateDirectory(options.DataFolder);
        var logPath = Path.Combine(options.DataFolder, "download.log");
        await using var log = new StreamWriter(logPath, append: false);
        try
        {
            using var downloader = new GameDownloader(new OfflineStore(options.DataFolder));
            string last = "";
            var progress = new Progress<DownloadProgress>(p =>
            {
                if (p.Message != last) log.WriteLine(p.Message);
                last = p.Message;
            });
            var summary = await downloader.DownloadAllAsync(GameCatalog.Games, progress, CancellationToken.None);
            await Task.Delay(200); // deixa o Progress<T> escrever as últimas linhas
            log.WriteLine($"Descarregados: {summary.Downloaded}; já existentes: {summary.AlreadyPresent}; inexistentes no servidor: {summary.Missing}");
            foreach (var err in summary.Errors) log.WriteLine("ERRO " + err);
            return summary.Errors.Count == 0 ? 0 : 1;
        }
        catch (Exception ex)
        {
            log.WriteLine("FALHOU: " + ex);
            return 2;
        }
    }
}
