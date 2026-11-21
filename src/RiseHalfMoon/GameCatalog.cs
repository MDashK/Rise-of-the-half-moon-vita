namespace RiseHalfMoon;

/// <summary>Um dos 8 jogos "Rise of the Half Moon" (Google Doodle, out/2024 – mai/2025).</summary>
public sealed record GameInfo(int Number, int Year, string HtmlPath, string ThumbnailPath, string DoodleSlug)
{
    public const string Origin = "https://www.google.com";

    public string Title => $"{Strings.Month(Number)} {Year}";

    /// <summary>URL original do jogo (servido localmente pela app, nunca pela rede).</summary>
    public Uri HtmlUri => new(Origin + HtmlPath);

    /// <summary>Pasta do jogo no servidor original, ex.: /logos/2024/moon/r3/</summary>
    public string BasePath => HtmlPath[..(HtmlPath.LastIndexOf('/') + 1)];

    public Uri ThumbnailUri => new(Origin + ThumbnailPath);

    public Uri DoodlePageUri => new($"https://doodles.google/doodle/{DoodleSlug}/");

    /// <summary>Ficheiros mínimos que têm de existir para o jogo correr offline.</summary>
    public IEnumerable<Uri> RequiredFiles()
    {
        yield return HtmlUri;
        yield return new Uri(Origin + BasePath + "cards-sprite.png");
        yield return new Uri(Origin + BasePath + "audio.mp3");
    }
}

public static class GameCatalog
{
    public static readonly IReadOnlyList<GameInfo> Games =
    [
        new(1, 2024, "/logos/2024/moon/r3/moon.html",
            "/logos/doodles/2024/rise-of-the-half-moon-6753651837110583.8-l.png",
            "rise-of-the-half-moon"),
        new(2, 2024, "/logos/2024/moon/novr2/moon.html",
            "/logos/doodles/2024/rise-of-the-half-moon-november-6753651837110598-l.png",
            "rise-of-the-half-moon-november"),
        new(3, 2024, "/logos/2024/moon/december-r4/december.html",
            "/logos/doodles/2024/rise-of-the-half-moon-december-6753651837110600.2-l.png",
            "rise-of-the-half-moon-december"),
        new(4, 2025, "/logos/2024/moon/moon_january-r1/moon_january.html",
            "/logos/doodles/2025/celebrating-the-rise-of-the-half-moon-jan-6753651837110604-l.png",
            "rise-of-the-half-moon-january"),
        new(5, 2025, "/logos/2024/moon/moon_february-r4/moon_february.html",
            "/logos/doodles/2025/celebrating-the-rise-of-the-half-moon-feb-6753651837110610.4-l.png",
            "rise-of-the-half-moon-february"),
        new(6, 2025, "/logos/2024/moon/moon_march-rc2/moon_march.html",
            "/logos/doodles/2025/rise-of-the-half-moon-march-6753651837110631.2-l.png",
            "rise-of-the-half-moon-march"),
        new(7, 2025, "/logos/2024/moon/moon_april-rc2/moon_april.html",
            "/logos/doodles/2025/rise-of-the-half-moon-april-6753651837110767.2-l.png",
            "rise-of-the-half-moon-april"),
        new(8, 2025, "/logos/2024/moon/moon_may-rc3/moon_may.html",
            "/logos/doodles/2025/rise-of-the-half-moon-may-6753651837110778.2-l.png",
            "rise-of-the-half-moon-may"),
    ];

    /// <summary>Línguas oferecidas no menu (todas usam fontes latinas, que são descarregadas).</summary>
    public static readonly IReadOnlyList<(string Code, string Name)> Languages =
    [
        ("pt-PT", "Português (Portugal)"),
        ("pt-BR", "Português (Brasil)"),
        ("en", "English"),
        ("es", "Español"),
        ("fr", "Français"),
        ("de", "Deutsch"),
        ("it", "Italiano"),
        ("nl", "Nederlands"),
    ];

    /// <summary>Recursos externos partilhados pelos jogos (carregador e folhas de fontes).</summary>
    public static readonly IReadOnlyList<string> SharedUrls =
    [
        "https://ajax.googleapis.com/ajax/libs/webfont/1/webfont.js",
        "https://fonts.googleapis.com/css?family=Josefin+Sans:200%7CGoogle+Sans%7CGoogle+Sans+Text",
        "https://fonts.googleapis.com/css?family=Josefin+Sans:200",
        "https://fonts.googleapis.com/css?family=Google+Sans",
        "https://fonts.googleapis.com/css?family=Google+Sans+Text",
    ];
}
