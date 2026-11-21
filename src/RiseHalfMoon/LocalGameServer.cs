using System.IO;
using System.Text;
using Microsoft.Web.WebView2.Core;

namespace RiseHalfMoon;

/// <summary>
/// Responde a todos os pedidos do WebView2 a partir da pasta local. Nenhum pedido sai para a
/// internet, excepto (opcionalmente) para completar ficheiros de jogo em falta.
/// </summary>
public sealed class LocalGameServer(OfflineStore store, CoreWebView2Environment environment, GameDownloader downloader)
{
    /// <summary>Se verdadeiro e houver internet, ficheiros de jogo em falta são descarregados e guardados.</summary>
    public bool FillMissingFromNetwork { get; set; } = true;

    public event Action<string>? MissingFile;

    public void Attach(CoreWebView2 core)
    {
        core.AddWebResourceRequestedFilter("*", CoreWebView2WebResourceContext.All);
        core.WebResourceRequested += OnWebResourceRequested;
    }

    private async void OnWebResourceRequested(object? sender, CoreWebView2WebResourceRequestedEventArgs e)
    {
        if (!Uri.TryCreate(e.Request.Uri, UriKind.Absolute, out var uri)
            || (uri.Scheme != Uri.UriSchemeHttps && uri.Scheme != Uri.UriSchemeHttp))
            return; // data:, blob:, about: — tratados pelo próprio browser

        var deferral = e.GetDeferral();
        try
        {
            e.Response = await CreateResponseAsync(e.Request, uri);
        }
        catch (Exception ex)
        {
            e.Response = Text(500, "Internal Server Error", ex.Message);
        }
        finally
        {
            deferral.Complete();
        }
    }

    private async Task<CoreWebView2WebResourceResponse> CreateResponseAsync(CoreWebView2WebResourceRequest request, Uri uri)
    {
        if (OfflineStore.IsTelemetry(uri))
            return environment.CreateWebResourceResponse(null, 204, "No Content", CorsHeaders);

        if (!OfflineStore.IsMirrorable(uri))
            return Text(404, "Not Found", "Bloqueado: a app funciona apenas offline.");

        var path = store.GetFilePath(uri);
        if (!File.Exists(path) && FillMissingFromNetwork)
        {
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(8));
            try { await downloader.FetchSingleAsync(uri, timeout.Token); }
            catch { /* sem internet: continua offline */ }
        }

        if (!File.Exists(path))
        {
            if (OfflineStore.NormalizeHost(uri.Host) == "fonts.googleapis.com" && store.GetMergedFontCss() is { } css)
                return Bytes(Encoding.UTF8.GetBytes(css), "text/css; charset=utf-8", request);

            MissingFile?.Invoke(uri.ToString());
            return Text(404, "Not Found", "Ficheiro não descarregado: " + uri);
        }

        var data = await File.ReadAllBytesAsync(path);
        return Bytes(data, OfflineStore.GetContentType(uri, path), request);
    }

    private const string CorsHeaders = "Access-Control-Allow-Origin: *";

    private CoreWebView2WebResourceResponse Bytes(byte[] data, string contentType, CoreWebView2WebResourceRequest request)
    {
        var headers = new StringBuilder()
            .Append("Content-Type: ").Append(contentType).Append("\r\n")
            .Append(CorsHeaders).Append("\r\n")
            .Append("Accept-Ranges: bytes\r\n")
            .Append("Cache-Control: no-cache");

        // Pedidos parciais (o vídeo de introdução usa "Range: bytes=...").
        if (request.Headers.Contains("Range") && TryParseRange(request.Headers.GetHeader("Range"), data.Length, out var from, out var to))
        {
            int length = (int)(to - from + 1);
            headers.Append("\r\nContent-Range: bytes ").Append(from).Append('-').Append(to).Append('/').Append(data.Length)
                   .Append("\r\nContent-Length: ").Append(length);
            return environment.CreateWebResourceResponse(
                new MemoryStream(data, (int)from, length, writable: false), 206, "Partial Content", headers.ToString());
        }

        headers.Append("\r\nContent-Length: ").Append(data.Length);
        return environment.CreateWebResourceResponse(new MemoryStream(data, writable: false), 200, "OK", headers.ToString());
    }

    private CoreWebView2WebResourceResponse Text(int status, string reason, string body) =>
        environment.CreateWebResourceResponse(
            new MemoryStream(Encoding.UTF8.GetBytes(body)), status, reason,
            "Content-Type: text/plain; charset=utf-8\r\n" + CorsHeaders);

    private static bool TryParseRange(string header, long size, out long from, out long to)
    {
        from = 0; to = size - 1;
        if (size == 0 || !header.StartsWith("bytes=", StringComparison.OrdinalIgnoreCase)) return false;
        var spec = header[6..].Split(',')[0].Trim();
        var dash = spec.IndexOf('-');
        if (dash < 0) return false;

        var a = spec[..dash].Trim();
        var b = spec[(dash + 1)..].Trim();
        if (a.Length == 0)
        {
            // "bytes=-500": últimos 500 bytes
            if (!long.TryParse(b, out var suffix) || suffix <= 0) return false;
            from = Math.Max(0, size - suffix);
        }
        else
        {
            if (!long.TryParse(a, out from) || from >= size) return false;
            if (b.Length > 0 && long.TryParse(b, out var end)) to = Math.Min(end, size - 1);
        }
        return from <= to;
    }
}
