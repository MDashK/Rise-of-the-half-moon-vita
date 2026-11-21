using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace RiseHalfMoon;

/// <summary>
/// Espelho local dos ficheiros dos jogos. Cada URL corresponde a um ficheiro em
/// &lt;raiz&gt;/&lt;host&gt;/&lt;caminho&gt;, por isso a pasta pode ser copiada para outro PC tal como está.
/// </summary>
public sealed class OfflineStore(string root)
{
    public string Root { get; } = Path.GetFullPath(root);

    /// <summary>Hosts cujos ficheiros são guardados (tudo o resto é bloqueado).</summary>
    private static readonly HashSet<string> MirroredHosts =
    [
        "www.google.com", "fonts.googleapis.com", "fonts.gstatic.com", "ajax.googleapis.com",
    ];

    public static string NormalizeHost(string host)
    {
        host = host.ToLowerInvariant();
        return host == "google.com" ? "www.google.com" : host;
    }

    /// <summary>Indica se o URL faz parte do que a app guarda (jogos, fontes, carregador de fontes).</summary>
    public static bool IsMirrorable(Uri uri)
    {
        if (uri.Scheme != Uri.UriSchemeHttps && uri.Scheme != Uri.UriSchemeHttp) return false;
        var host = NormalizeHost(uri.Host);
        if (!MirroredHosts.Contains(host)) return false;
        return host != "www.google.com" || uri.AbsolutePath.StartsWith("/logos/", StringComparison.Ordinal);
    }

    /// <summary>Pedidos de estatísticas/telemetria do doodle: respondidos com 204 sem ir à rede.</summary>
    public static bool IsTelemetry(Uri uri)
    {
        var host = NormalizeHost(uri.Host);
        if (host != "www.google.com") return false;
        var path = uri.AbsolutePath;
        return path.StartsWith("/gen_204", StringComparison.Ordinal)
            || path.StartsWith("/async/", StringComparison.Ordinal)
            || path.StartsWith("/recaptcha/", StringComparison.Ordinal);
    }

    public string GetFilePath(Uri uri)
    {
        var host = NormalizeHost(uri.Host);
        var path = Uri.UnescapeDataString(uri.AbsolutePath).TrimStart('/');
        if (path.Length == 0 || path.EndsWith('/')) path += "index";

        var segments = path.Split('/', StringSplitOptions.RemoveEmptyEntries)
            .Select(Sanitize)
            .ToArray();

        // Nos jogos a query (?hl=pt-PT) não muda o ficheiro; nas folhas de fontes muda.
        bool queryMatters = host != "www.google.com" && uri.Query.Length > 1;
        if (queryMatters)
        {
            var last = segments[^1];
            var ext = Path.GetExtension(last);
            segments[^1] = Path.GetFileNameWithoutExtension(last) + "__q" + QueryHash(uri.Query) + ext;
        }

        return Path.Combine([Root, host, .. segments]);
    }

    public bool Exists(Uri uri) => File.Exists(GetFilePath(uri));

    public static string GetContentType(Uri uri, string filePath)
    {
        if (NormalizeHost(uri.Host) == "fonts.googleapis.com") return "text/css; charset=utf-8";
        return Path.GetExtension(filePath).ToLowerInvariant() switch
        {
            ".html" or ".htm" => "text/html; charset=utf-8",
            ".js" => "text/javascript; charset=utf-8",
            ".json" => "application/json; charset=utf-8",
            ".css" => "text/css; charset=utf-8",
            ".png" => "image/png",
            ".gif" => "image/gif",
            ".jpg" or ".jpeg" => "image/jpeg",
            ".webp" => "image/webp",
            ".svg" => "image/svg+xml",
            ".mp4" => "video/mp4",
            ".webm" => "video/webm",
            ".mp3" => "audio/mpeg",
            ".ogg" => "audio/ogg",
            ".wav" => "audio/wav",
            ".woff2" => "font/woff2",
            ".woff" => "font/woff",
            ".ttf" => "font/ttf",
            _ => "application/octet-stream",
        };
    }

    /// <summary>Todas as folhas de fontes guardadas, juntas (usado se o jogo pedir uma combinação não guardada).</summary>
    public string? GetMergedFontCss()
    {
        var dir = Path.Combine(Root, "fonts.googleapis.com");
        if (!Directory.Exists(dir)) return null;
        var sb = new StringBuilder();
        foreach (var file in Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories).Order())
            sb.AppendLine(File.ReadAllText(file));
        return sb.Length == 0 ? null : sb.ToString();
    }

    private static string QueryHash(string query)
    {
        var normalized = Uri.UnescapeDataString(query.TrimStart('?')).Replace('+', ' ');
        var hash = SHA1.HashData(Encoding.UTF8.GetBytes(normalized));
        return Convert.ToHexString(hash, 0, 5).ToLowerInvariant();
    }

    private static string Sanitize(string segment)
    {
        var invalid = Path.GetInvalidFileNameChars();
        var chars = segment.Select(c => invalid.Contains(c) ? '_' : c).ToArray();
        var result = new string(chars);
        return result is "." or ".." ? "_" : result;
    }
}
