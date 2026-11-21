using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using Microsoft.Web.WebView2.Core;

namespace RiseHalfMoon;

public sealed class GameCard(GameInfo game) : INotifyPropertyChanged
{
    private static readonly Brush AvailableBrush = Freeze(new SolidColorBrush(Color.FromRgb(0xB8, 0xE0, 0xB0)));
    private static readonly Brush MissingBrush = Freeze(new SolidColorBrush(Color.FromRgb(0xE8, 0xB4, 0x8C)));

    public GameInfo Game { get; } = game;
    public string Title => Game.Title;
    public string Subtitle => Strings.F("game_n_of_8", Game.Number);
    public ImageSource? Thumbnail { get; private set; }
    public bool IsAvailable { get; private set; }
    public string StatusText => IsAvailable ? "✓ " + Strings.T("available") : Strings.T("not_downloaded");
    public Brush StatusBrush => IsAvailable ? AvailableBrush : MissingBrush;
    public Visibility PlaceholderVisibility => Thumbnail is null ? Visibility.Visible : Visibility.Collapsed;
    public string Tooltip => $"Rise of the Half Moon — {Game.Title}\n{Strings.T("original")}: {Game.DoodlePageUri}";

    public event PropertyChangedEventHandler? PropertyChanged;

    public void Refresh(OfflineStore store)
    {
        IsAvailable = Game.RequiredFiles().All(store.Exists);
        Thumbnail = LoadImage(store.GetFilePath(Game.ThumbnailUri));
        NotifyAll();
    }

    /// <summary>Volta a ler todos os textos (depois de mudar de língua).</summary>
    public void NotifyAll() => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(null));

    private static ImageSource? LoadImage(string path)
    {
        if (!File.Exists(path)) return null;
        try
        {
            var image = new BitmapImage();
            image.BeginInit();
            image.CacheOption = BitmapCacheOption.OnLoad; // não bloqueia o ficheiro
            image.UriSource = new Uri(path);
            image.EndInit();
            image.Freeze();
            return image;
        }
        catch { return null; }
    }

    private static Brush Freeze(Brush b) { b.Freeze(); return b; }
}

public partial class MainWindow : Window
{
    private readonly AppOptions _options;
    private readonly OfflineStore _store;
    private readonly GameDownloader _downloader;
    private readonly ObservableCollection<GameCard> _cards;
    private readonly string _settingsFolder =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "RiseHalfMoonOffline");

    private Task? _webViewReady;
    private GameInfo? _currentGame;
    private CancellationTokenSource? _downloadCts;
    private bool _fullscreen;
    private WindowState _stateBeforeFullscreen;

    public MainWindow(AppOptions options)
    {
        InitializeComponent();
        var work = SystemParameters.WorkArea;
        Width = Math.Min(Width, work.Width - 40);
        Height = Math.Min(Height, work.Height - 40);
        _options = options;
        _store = new OfflineStore(options.DataFolder);
        _downloader = new GameDownloader(_store);
        _cards = new(GameCatalog.Games.Select(g => new GameCard(g)));
        GamesList.ItemsSource = _cards;

        LanguageBox.ItemsSource = GameCatalog.Languages.Select(l => new LanguageItem(l.Code, l.Name)).ToList();
        var saved = ReadSetting("language") ?? Strings.DefaultCode();
        Strings.SetLanguage(saved);
        LanguageBox.SelectedItem = ((List<LanguageItem>)LanguageBox.ItemsSource).FirstOrDefault(l => l.Code == saved)
                                   ?? ((List<LanguageItem>)LanguageBox.ItemsSource)[0];

        ApplyLanguage();
        Loaded += OnLoaded;
        Closed += (_, _) => _downloader.Dispose();
    }

    private sealed record LanguageItem(string Code, string Name);

    private string GameLanguage => (LanguageBox.SelectedItem as LanguageItem)?.Code ?? Strings.Code;

    /// <summary>Aplica a língua escolhida a todos os textos da interface.</summary>
    private void ApplyLanguage()
    {
        SubtitleText.Text = Strings.T("subtitle");
        LanguageLabel.Text = Strings.T("language");
        DownloadButton.Content = _downloadCts is null ? "⬇  " + Strings.T("download_update") : "✕  " + Strings.T("cancel");
        AboutButton.ToolTip = Strings.T("about");
        OpenFolderButton.Content = "📁  " + Strings.T("open_folder");
        MenuButton.Content = "◀  " + Strings.T("menu");
        RestartButton.Content = "↻  " + Strings.T("restart");
        FullscreenButton.Content = _fullscreen ? "✕  " + Strings.T("exit_fullscreen") : "⛶  " + Strings.T("fullscreen");
        foreach (var card in _cards) card.NotifyAll();
        if (_downloadCts is null) RefreshStatus();
    }

    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        RefreshCards();
        _webViewReady = InitWebViewAsync();

        if (_options.AutoPlay is int n && GameCatalog.Games.FirstOrDefault(g => g.Number == n) is { } game)
            await PlayAsync(game);
    }

    private void RefreshCards()
    {
        foreach (var card in _cards) card.Refresh(_store);
        RefreshStatus();
    }

    private void RefreshStatus()
    {
        int available = _cards.Count(c => c.IsAvailable);
        StatusText.Text = available == _cards.Count
            ? Strings.T("all_available")
            : Strings.F("some_available", available, _cards.Count, Strings.T("download_update"));
    }

    // ---------------------------------------------------------------- WebView2

    private async Task InitWebViewAsync()
    {
        try
        {
            var env = await CoreWebView2Environment.CreateAsync(
                browserExecutableFolder: null,
                userDataFolder: Path.Combine(_settingsFolder, "WebView2"),
                options: new CoreWebView2EnvironmentOptions("--autoplay-policy=no-user-gesture-required"));
            await WebView.EnsureCoreWebView2Async(env);

            var core = WebView.CoreWebView2;
            core.Settings.AreDefaultContextMenusEnabled = false;
            core.Settings.AreDevToolsEnabled = Debugger.IsAttached;
            core.Settings.IsStatusBarEnabled = false;
            core.Settings.IsZoomControlEnabled = false;
            core.Settings.AreBrowserAcceleratorKeysEnabled = false;

            var server = new LocalGameServer(_store, env, _downloader) { FillMissingFromNetwork = !_options.StrictOffline };
            server.MissingFile += url => Debug.WriteLine("Em falta: " + url);
            server.Attach(core);

            core.NavigationStarting += OnNavigationStarting;
            core.NewWindowRequested += OnNewWindowRequested;
        }
        catch (WebView2RuntimeNotFoundException)
        {
            MessageBox.Show(this,
                Strings.F("webview_missing", "https://developer.microsoft.com/microsoft-edge/webview2/"),
                Strings.T("webview_missing_title"), MessageBoxButton.OK, MessageBoxImage.Error);
            throw;
        }
    }

    private void OnNavigationStarting(object? sender, CoreWebView2NavigationStartingEventArgs e)
    {
        if (!Uri.TryCreate(e.Uri, UriKind.Absolute, out var uri)) return;
        if (uri.Scheme == "about" || uri.Scheme == "data") return;
        if (OfflineStore.IsMirrorable(uri) && OfflineStore.NormalizeHost(uri.Host) == "www.google.com") return;

        // Qualquer outro destino (pesquisa Google, partilhas, ...) sai da app: abre no browser normal.
        e.Cancel = true;
        OpenExternal(uri);
    }

    private void OnNewWindowRequested(object? sender, CoreWebView2NewWindowRequestedEventArgs e)
    {
        e.Handled = true;
        if (Uri.TryCreate(e.Uri, UriKind.Absolute, out var uri)) OpenExternal(uri);
    }

    private static void OpenExternal(Uri uri)
    {
        if (uri.Scheme != Uri.UriSchemeHttps && uri.Scheme != Uri.UriSchemeHttp) return;
        try { Process.Start(new ProcessStartInfo(uri.ToString()) { UseShellExecute = true }); }
        catch { /* sem browser predefinido */ }
    }

    // ---------------------------------------------------------------- Navegação menu/jogo

    private async void OnGameCardClick(object sender, RoutedEventArgs e)
    {
        if ((sender as FrameworkElement)?.Tag is GameCard card) await PlayAsync(card.Game);
    }

    private async Task PlayAsync(GameInfo game)
    {
        var card = _cards.First(c => c.Game == game);
        card.Refresh(_store);
        if (!card.IsAvailable)
        {
            var answer = MessageBox.Show(this, Strings.F("ask_download", game.Title),
                Strings.T("not_downloaded_title"), MessageBoxButton.YesNo, MessageBoxImage.Question);
            if (answer != MessageBoxResult.Yes) return;
            await DownloadAsync([game]);
            card.Refresh(_store);
            if (!card.IsAvailable) return;
        }

        if (_webViewReady is null) return;
        try { await _webViewReady; } catch { return; }

        _currentGame = game;
        GameTitle.Text = $"Rise of the Half Moon — {game.Title}";
        Title = $"{game.Title} — Rise of the Half Moon";
        MenuView.Visibility = Visibility.Collapsed;
        GameView.Visibility = Visibility.Visible;
        WebView.CoreWebView2.Navigate($"{game.HtmlUri}?hl={Uri.EscapeDataString(GameLanguage)}");
        WebView.Focus();
    }

    private void ShowMenu()
    {
        if (_fullscreen) ToggleFullscreen();
        WebView.CoreWebView2?.Navigate("about:blank"); // pára som e animações
        _currentGame = null;
        Title = "Rise of the Half Moon";
        GameView.Visibility = Visibility.Collapsed;
        MenuView.Visibility = Visibility.Visible;
        RefreshCards();
    }

    private void OnBackClick(object sender, RoutedEventArgs e) => ShowMenu();

    private void OnReloadClick(object sender, RoutedEventArgs e)
    {
        if (_currentGame is not null)
            WebView.CoreWebView2?.Navigate($"{_currentGame.HtmlUri}?hl={Uri.EscapeDataString(GameLanguage)}");
    }

    private void OnFullscreenClick(object sender, RoutedEventArgs e) => ToggleFullscreen();

    private void ToggleFullscreen()
    {
        _fullscreen = !_fullscreen;
        if (_fullscreen)
        {
            _stateBeforeFullscreen = WindowState;
            WindowStyle = WindowStyle.None;
            ResizeMode = ResizeMode.NoResize;
            WindowState = WindowState.Normal; // necessário para cobrir a barra de tarefas
            WindowState = WindowState.Maximized;
            FullscreenButton.Content = "✕  " + Strings.T("exit_fullscreen");
        }
        else
        {
            WindowStyle = WindowStyle.SingleBorderWindow;
            ResizeMode = ResizeMode.CanResize;
            WindowState = _stateBeforeFullscreen;
            FullscreenButton.Content = "⛶  " + Strings.T("fullscreen");
        }
    }

    private void OnPreviewKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.F11 && GameView.Visibility == Visibility.Visible)
        {
            ToggleFullscreen();
            e.Handled = true;
        }
        else if (e.Key == Key.Escape && _fullscreen)
        {
            ToggleFullscreen();
            e.Handled = true;
        }
    }

    private void OnLanguageChanged(object sender, SelectionChangedEventArgs e)
    {
        if (LanguageBox.SelectedItem is not LanguageItem item) return;
        WriteSetting("language", item.Code);
        Strings.SetLanguage(item.Code);
        if (_cards is not null && SubtitleText is not null) ApplyLanguage();
    }

    private void OnAboutClick(object sender, RoutedEventArgs e) => new AboutWindow { Owner = this }.ShowDialog();

    // ---------------------------------------------------------------- Descarregar

    private async void OnDownloadClick(object sender, RoutedEventArgs e)
    {
        if (_downloadCts is not null)
        {
            _downloadCts.Cancel();
            return;
        }
        await DownloadAsync(GameCatalog.Games);
    }

    private async Task DownloadAsync(IReadOnlyList<GameInfo> games)
    {
        _downloadCts = new CancellationTokenSource();
        DownloadButton.Content = "✕  " + Strings.T("cancel");
        DownloadProgressBar.Visibility = Visibility.Visible;
        DownloadProgressBar.Value = 0;
        var progress = new Progress<DownloadProgress>(p =>
        {
            StatusText.Text = p.Message;
            DownloadProgressBar.Maximum = p.Total;
            DownloadProgressBar.Value = p.Done;
        });

        try
        {
            var summary = await _downloader.DownloadAllAsync(games, progress, _downloadCts.Token);
            RefreshCards();
            var text = Strings.F("dl_summary", summary.Downloaded, summary.AlreadyPresent);
            if (summary.Errors.Count > 0)
            {
                text += Strings.F("dl_errors", summary.Errors.Count);
                MessageBox.Show(this, string.Join("\n", summary.Errors.Take(15)), Strings.T("dl_failed_title"),
                    MessageBoxButton.OK, MessageBoxImage.Warning);
            }
            StatusText.Text = text;
        }
        catch (OperationCanceledException)
        {
            RefreshCards();
            StatusText.Text = Strings.T("dl_cancelled");
        }
        catch (Exception ex)
        {
            RefreshCards();
            StatusText.Text = Strings.F("dl_failed", ex.Message);
        }
        finally
        {
            _downloadCts.Dispose();
            _downloadCts = null;
            DownloadButton.Content = "⬇  " + Strings.T("download_update");
            DownloadProgressBar.Visibility = Visibility.Collapsed;
        }
    }

    private void OnOpenFolderClick(object sender, RoutedEventArgs e)
    {
        Directory.CreateDirectory(_store.Root);
        Process.Start(new ProcessStartInfo(_store.Root) { UseShellExecute = true });
    }

    // ---------------------------------------------------------------- Definições

    private string? ReadSetting(string name)
    {
        try { return File.ReadAllText(Path.Combine(_settingsFolder, name + ".txt")).Trim(); }
        catch { return null; }
    }

    private void WriteSetting(string name, string value)
    {
        try
        {
            Directory.CreateDirectory(_settingsFolder);
            File.WriteAllText(Path.Combine(_settingsFolder, name + ".txt"), value);
        }
        catch { /* não é crítico */ }
    }
}
