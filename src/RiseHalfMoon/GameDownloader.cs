using System.IO;
using System.Net;
using System.Net.Http;
using System.Text.RegularExpressions;

namespace RiseHalfMoon;

public sealed record DownloadProgress(string Message, int Done, int Total);

public sealed class DownloadSummary
{
    public int Downloaded { get; set; }
    public int AlreadyPresent { get; set; }
    public int Missing { get; set; }
    public List<string> Errors { get; } = [];
}

/// <summary>
/// Descarrega tudo o que cada jogo precisa: HTML, JavaScript, imagens, vídeo, áudio,
/// traduções e fontes. Os ficheiros já existentes não são descarregados de novo.
/// </summary>
public sealed partial class GameDownloader : IDisposable
{
    // Agente de utilizador de um Chrome recente, para o Google Fonts devolver ficheiros woff2.
    public const string UserAgent =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

    private readonly OfflineStore _store;
    private readonly HttpClient _http;

    public GameDownloader(OfflineStore store)
    {
        _store = store;
        _http = new HttpClient(new HttpClientHandler { AutomaticDecompression = DecompressionMethods.All })
        {
            Timeout = TimeSpan.FromSeconds(60),
        };
        _http.DefaultRequestHeaders.UserAgent.ParseAdd(UserAgent);
        _http.DefaultRequestHeaders.Referrer = new Uri("https://www.google.com/");
    }

    public void Dispose() => _http.Dispose();

    [GeneratedRegex(@"/logos/[^""'\s()<>\\`]+")]
    private static partial Regex AbsoluteLogoPath();

    [GeneratedRegex(@"[""'`]([A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:png|jpe?g|gif|webp|svg|mp4|webm|mp3|ogg|wav|json))[""'`]")]
    private static partial Regex RelativeAssetName();

    [GeneratedRegex(@"""([A-Za-z]{2,3}(?:-[A-Za-z0-9]{2,4})?)"":\{""ALL""")]
    private static partial Regex L10nLanguage();

    [GeneratedRegex(@"url\(\s*['""]?(https?://[^)'""]+)['""]?\s*\)")]
    private static partial Regex CssUrl();

    public async Task<DownloadSummary> DownloadAllAsync(IEnumerable<GameInfo> games,
        IProgress<DownloadProgress>? progress, CancellationToken ct)
    {
        var summary = new DownloadSummary();
        var gameList = games.ToList();
        int done = 0, total = gameList.Count + 1;

        foreach (var game in gameList)
        {
            progress?.Report(new(Strings.F("checking_game", game.Title), done, total));
            var urls = await DiscoverGameFilesAsync(game, summary, ct);
            int i = 0;
            foreach (var url in urls)
            {
                ct.ThrowIfCancellationRequested();
                i++;
                progress?.Report(new($"{game.Title}: {i}/{urls.Count} — {Path.GetFileName(url.AbsolutePath)}", done, total));
                await FetchAsync(url, summary, optional: IsOptional(url), ct);
            }
            done++;
        }

        progress?.Report(new(Strings.T("downloading_fonts"), done, total));
        foreach (var shared in GameCatalog.SharedUrls)
        {
            var uri = new Uri(shared);
            var path = await FetchAsync(uri, summary, optional: false, ct);
            if (path is not null && uri.Host == "fonts.googleapis.com")
            {
                var css = await File.ReadAllTextAsync(path, ct);
                foreach (Match m in CssUrl().Matches(css))
                    await FetchAsync(new Uri(m.Groups[1].Value), summary, optional: false, ct);
            }
        }
        done++;
        progress?.Report(new(Strings.T("done"), done, total));
        return summary;
    }

    /// <summary>Obtém um único ficheiro em falta (usado durante o jogo, se houver internet).</summary>
    public async Task<string?> FetchSingleAsync(Uri uri, CancellationToken ct)
    {
        var summary = new DownloadSummary();
        return await FetchAsync(uri, summary, optional: true, ct);
    }

    private async Task<List<Uri>> DiscoverGameFilesAsync(GameInfo game, DownloadSummary summary, CancellationToken ct)
    {
        var found = new SortedSet<string>(StringComparer.Ordinal);
        var origin = GameInfo.Origin;

        var htmlPath = await FetchAsync(game.HtmlUri, summary, optional: false, ct)
            ?? throw new InvalidOperationException($"Não foi possível obter {game.HtmlUri}");
        var html = await File.ReadAllTextAsync(htmlPath, ct);

        var scripts = new List<Uri>();
        foreach (Match m in AbsoluteLogoPath().Matches(html))
        {
            var p = m.Value;
            if (p.EndsWith('/')) continue;
            found.Add(origin + p);
            if (p.EndsWith(".js", StringComparison.OrdinalIgnoreCase)) scripts.Add(new Uri(origin + p));
        }

        foreach (var script in scripts)
        {
            var jsPath = await FetchAsync(script, summary, optional: false, ct);
            if (jsPath is null) continue;
            var js = await File.ReadAllTextAsync(jsPath, ct);

            foreach (Match m in AbsoluteLogoPath().Matches(js))
                if (!m.Value.EndsWith('/') && Path.HasExtension(m.Value)) found.Add(origin + m.Value);

            foreach (Match m in RelativeAssetName().Matches(js))
                found.Add(origin + game.BasePath + m.Groups[1].Value);
        }

        // Áudio: o nome é montado em tempo de execução ("audio" + ".ogg"/".mp3").
        found.Add(origin + game.BasePath + "audio.mp3");
        found.Add(origin + game.BasePath + "audio.ogg");
        found.Add(game.ThumbnailUri.ToString());

        // Traduções: messages.<língua>.nocache.json para todas as línguas que o doodle suporta.
        var langs = new SortedSet<string>(StringComparer.Ordinal) { "en", "pt-PT", "pt-BR" };
        foreach (Match m in L10nLanguage().Matches(html)) langs.Add(m.Groups[1].Value);
        foreach (var lang in langs)
            found.Add(origin + game.BasePath + $"messages.{lang}.nocache.json");

        found.Remove(game.HtmlUri.ToString());
        return found.Select(u => new Uri(u)).ToList();
    }

    // Ficheiros que podem legitimamente não existir no servidor (traduções de algumas línguas, formatos alternativos).
    private static bool IsOptional(Uri url) =>
        url.AbsolutePath.EndsWith(".nocache.json", StringComparison.Ordinal)
        || url.AbsolutePath.EndsWith(".json", StringComparison.Ordinal)
        || url.AbsolutePath.EndsWith("audio.ogg", StringComparison.Ordinal);

    private async Task<string?> FetchAsync(Uri uri, DownloadSummary summary, bool optional, CancellationToken ct)
    {
        var path = _store.GetFilePath(uri);
        if (File.Exists(path))
        {
            summary.AlreadyPresent++;
            return path;
        }

        try
        {
            using var response = await _http.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead, ct);
            if (!response.IsSuccessStatusCode)
            {
                if (!optional) summary.Errors.Add($"{(int)response.StatusCode} {uri}");
                summary.Missing++;
                return null;
            }

            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            var tmp = path + ".part";
            await using (var file = File.Create(tmp))
                await response.Content.CopyToAsync(file, ct);
            File.Move(tmp, path, overwrite: true);
            summary.Downloaded++;
            return path;
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException && !ct.IsCancellationRequested)
        {
            summary.Errors.Add($"{uri}: {ex.Message}");
            return null;
        }
    }
}
