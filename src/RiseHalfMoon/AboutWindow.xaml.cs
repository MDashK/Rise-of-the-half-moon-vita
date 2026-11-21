using System.Reflection;
using System.Windows;
using System.Windows.Input;

namespace RiseHalfMoon;

/// <summary>Caixa "About": créditos, avisos de direitos e licenças (o mesmo conteúdo da versão PS Vita).</summary>
public partial class AboutWindow : Window
{
    public AboutWindow()
    {
        InitializeComponent();
        var v = Assembly.GetExecutingAssembly().GetName().Version ?? new Version(1, 0, 0);
        Title = Strings.T("about");
        VersionText.Text = $"{Strings.T("about_pc")}  ·  v{v.Major}.{v.Minor}.{v.Build}";
        GoogleText.Text = Strings.T("about_google");
        UnofficialText.Text = Strings.T("about_unofficial");
        ThirdPartyTitle.Text = Strings.T("third_party");
        ThirdPartyList.ItemsSource = new[]
        {
            "Microsoft Edge WebView2 SDK © Microsoft Corporation — BSD-3-Clause License",
            ".NET / WPF © .NET Foundation and Contributors — MIT License",
            "Josefin Sans (downloaded with the games) © The Josefin Sans Project Authors — SIL Open Font License 1.1",
            "Google and Google Doodle are trademarks of Google LLC.",
            "Microsoft, Windows and Microsoft Edge are trademarks of the Microsoft group of companies.",
        };
        CloseButton.Content = Strings.T("close");
    }

    private void OnCloseClick(object sender, RoutedEventArgs e) => Close();

    private void OnKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape) Close();
    }
}
