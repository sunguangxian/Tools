using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace SscomAvalonia.ViewModels;

public partial class PresetItemViewModel : ObservableObject
{
    public int Index { get; }

    [ObservableProperty] private string _text = "";
    [ObservableProperty] private bool _hex;

    public string Label => $"{Index + 1}";

    public IRelayCommand SendCommand { get; }

    public PresetItemViewModel(int index, Action<PresetItemViewModel> onSend)
    {
        Index = index;
        SendCommand = new RelayCommand(() => onSend(this));
    }
}
