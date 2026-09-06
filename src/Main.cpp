#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

// Settings file for persisting state
static juce::File getSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("PluginStandalone")
        .getChildFile("settings.xml");
}

static juce::XmlElement* loadSettings()
{
    auto f = getSettingsFile();
    return f.existsAsFile() ? juce::XmlDocument::parse(f).release() : nullptr;
}

static void saveSettings(const juce::XmlElement& xml)
{
    auto f = getSettingsFile();
    f.getParentDirectory().createDirectory();
    xml.writeTo(f);
}

//==============================================================================
class PluginEditorWrapper : public juce::Component,
                            private juce::ComponentListener,
                            private juce::Timer
{
public:
    explicit PluginEditorWrapper(std::unique_ptr<juce::AudioProcessorEditor> ed)
        : editor(std::move(ed))
    {
        jassert(editor != nullptr);

        if (editor != nullptr)
        {
            editorWidth = juce::jmax(1, editor->getWidth());
            editorHeight = juce::jmax(1, editor->getHeight());
            editor->addComponentListener(this);
            editorCanResize = editor->isResizable();
            addAndMakeVisible(editor.get());
            setSize(editorWidth, editorHeight);
        }
    }

    ~PluginEditorWrapper() override
    {
        stopTimer();

        // AudioProcessorEditor is not owned by Component::addAndMakeVisible.
        // Destroy it explicitly before the plugin processor is unloaded.
        if (editor != nullptr)
        {
            editor->removeComponentListener(this);
            removeChildComponent(editor.get());
            editor.reset();
        }
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }
    void resized() override
    {
        if (editor != nullptr)
            if (editorCanResize)
            {
                juce::ScopedValueSetter<bool> guard(resizingWrapper, true);
                editor->setBounds(getLocalBounds());
            }
    }

private:
    void componentMovedOrResized(juce::Component& component,
                                 bool,
                                 bool wasResized) override
    {
        if (! wasResized || &component != editor.get() || resizingWrapper)
            return;

        const auto newWidth = juce::jmax(1, editor->getWidth());
        const auto newHeight = juce::jmax(1, editor->getHeight());

        if (newWidth == editorWidth && newHeight == editorHeight)
            return;

        editorWidth = newWidth;
        editorHeight = newHeight;

        // Plugins such as ScalerAudio can change their editor size when the
        // user changes an internal UI scale. Resize the wrapper so the
        // ResizableWindow's content-size tracking resizes the host window too.
        // Defer this work until the current plugin UI callback has returned so
        // a window resize cannot delay the click or MIDI event being handled.
        startTimer(1);
    }

    void timerCallback() override
    {
        stopTimer();

        if (getWidth() == editorWidth && getHeight() == editorHeight)
            return;

        juce::ScopedValueSetter<bool> guard(resizingWrapper, true);
        setSize(editorWidth, editorHeight);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor;
    int editorWidth = 1;
    int editorHeight = 1;
    bool editorCanResize = false;
    bool resizingWrapper = false;
};

//==============================================================================
class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow(std::unique_ptr<juce::AudioProcessorEditor> editor,
                 std::function<void()> onClose)
        : DocumentWindow("Plugin", juce::Colours::black, DocumentWindow::allButtons),
          closeCallback(std::move(onClose))
    {
        const auto initialWidth = juce::jmax(1, editor != nullptr ? editor->getWidth() : 640);
        const auto initialHeight = juce::jmax(1, editor != nullptr ? editor->getHeight() : 480);
        const auto editorCanResize = editor != nullptr && editor->isResizable();

        setBackgroundColour(juce::Colours::black);
        setUsingNativeTitleBar(true);
        setResizable(editorCanResize, editorCanResize);
        if (editorCanResize)
            setResizeLimits(initialWidth, initialHeight, initialWidth * 2, initialHeight * 2);
        setContentOwned(new PluginEditorWrapper(std::move(editor)), true);
        centreWithSize(initialWidth, initialHeight);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        // Do not destroy this DocumentWindow from inside its own callback.
        // Defer the owner reset until the current event has returned.
        if (closeCallback != nullptr)
            juce::MessageManager::callAsync(closeCallback);
    }

private:
    std::function<void()> closeCallback;
};

//==============================================================================
class HostAudioProcessor : public juce::AudioProcessor
{
public:
    HostAudioProcessor()
        : AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {
        // AudioProcessorPlayer duplicates a single selected hardware input
        // into both processor inputs, preserving stereo plugin output.
        return layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    const juce::String getName() const override { return "Host"; }
    void prepareToPlay(double sr, int bs) override
    {
        if (loaded)
            loaded->prepareToPlay(sr, bs);
    }
    void releaseResources() override
    {
        if (loaded)
            loaded->releaseResources();
    }
    void processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) override
    {
        if (loaded)
            loaded->processBlock(buf, midi);
        else
            buf.clear();
    }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    std::unique_ptr<juce::AudioProcessor> loaded;
};

//==============================================================================
class MainComponent : public juce::Component
{
public:
    MainComponent()
    {
        formatManager.addDefaultFormats();

        loadBtn.setButtonText("Load Plugin...");
        loadBtn.onClick = [this] { showPluginList(); };
        addAndMakeVisible(loadBtn);

        audioSettingsBtn.setButtonText("Audio Settings...");
        audioSettingsBtn.onClick = [this] { showAudioSettings(); };
        addAndMakeVisible(audioSettingsBtn);

        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setText("No plugin loaded", juce::dontSendNotification);
        addAndMakeVisible(statusLabel);

        setupAudio();
        restoreState();

        setSize(400, 120);
    }

    ~MainComponent() override
    {
        deviceManager.removeAudioCallback(&player);
        player.setProcessor(nullptr);
        closePluginWindow();

        // AudioProcessorPlayer::setProcessor(nullptr) has stopped the host
        // processor and released the loaded plugin's audio resources. State
        // can now be queried without an audio callback racing this thread.
        saveState();

        const juce::ScopedLock audioLock(hostProcessor.getCallbackLock());
        hostProcessor.loaded.reset();
        deviceManager.closeAudioDevice();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        auto top = area.removeFromTop(30);
        loadBtn.setBounds(top.removeFromLeft(top.getWidth() / 2).reduced(2));
        audioSettingsBtn.setBounds(top.reduced(2));
        area.removeFromTop(10);
        statusLabel.setBounds(area);
    }

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    juce::AudioDeviceManager deviceManager;
    HostAudioProcessor hostProcessor;
    juce::AudioProcessorPlayer player;
    std::unique_ptr<PluginWindow> pluginWindow;
    juce::String lastPluginId;

    juce::TextButton loadBtn, audioSettingsBtn;
    juce::Label statusLabel;

    void setupAudio()
    {
        auto savedSettings = std::unique_ptr<juce::XmlElement>(loadSettings());
        auto* audioState = savedSettings ? savedSettings->getChildByName("DEVICESETUP") : nullptr;

        deviceManager.initialise(2, 2, audioState, true);
        player.setProcessor(&hostProcessor);
        deviceManager.addAudioCallback(&player);
    }

    void showAudioSettings()
    {
        auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManager, 1, 2, 1, 2, false, false, false, false);
        selector->setSize(500, 300);

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(selector.release());
        opts.dialogTitle = "Audio Settings";
        opts.useNativeTitleBar = true;
        opts.resizable = false;
        opts.dialogBackgroundColour = juce::Colours::darkgrey;
        opts.launchAsync();
    }

    void scanPlugins()
    {
        for (auto* format : formatManager.getFormats())
        {
            auto paths = format->getDefaultLocationsToSearch();
#if JUCE_MAC
            paths.addIfNotAlreadyThere(juce::File("/Library/Audio/Plug-Ins/VST3"));
            paths.addIfNotAlreadyThere(juce::File("/Library/Audio/Plug-Ins/Components"));
#elif JUCE_WINDOWS
            paths.addIfNotAlreadyThere(juce::File("C:\\Program Files\\Common Files\\VST3"));
            paths.addIfNotAlreadyThere(juce::File("C:\\Program Files\\VSTPlugins"));
            paths.addIfNotAlreadyThere(juce::File("C:\\Program Files\\Steinberg\\VSTPlugins"));
#endif
            juce::PluginDirectoryScanner scanner(knownPlugins, *format, paths, true, juce::File());
            juce::String name;
            while (scanner.scanNextFile(true, name)) {}
        }
    }

    void showPluginList()
    {
        scanPlugins();

        // Build menu manually, preferring AU over VST3 when both exist
        juce::PopupMenu menu;
        auto types = knownPlugins.getTypes();

        // Collect plugin names that have AU versions
        juce::StringArray auNames;
        for (auto& t : types)
            if (t.pluginFormatName == "AudioUnit")
                auNames.add(t.name);

        // Filter: skip VST3 if AU version exists, skip Apple built-in AUs
        juce::Array<juce::PluginDescription> filtered;
        for (auto& t : types)
        {
            if (t.pluginFormatName == "VST3" && auNames.contains(t.name))
                continue;
            if (t.manufacturerName == "Apple")
                continue;
            filtered.add(t);
        }

        for (int i = 0; i < filtered.size(); ++i)
            menu.addItem(i + 1, filtered[i].name + " (" + filtered[i].pluginFormatName + ")");

        if (filtered.isEmpty())
        {
            statusLabel.setText("No plugins found", juce::dontSendNotification);
            return;
        }

        menu.showMenuAsync({}, [this, filtered = std::move(filtered)](int result) {
            if (result > 0 && result <= filtered.size())
                loadPlugin(filtered[result - 1]);
        });
    }

    void loadPlugin(const juce::PluginDescription& desc)
    {
        // Stop the host processor before replacing the plugin. This also
        // calls HostAudioProcessor::releaseResources() for the old instance.
        player.setProcessor(nullptr);
        closePluginWindow();

        {
            const juce::ScopedLock audioLock(hostProcessor.getCallbackLock());
            hostProcessor.loaded.reset();
        }

        juce::String error;
        auto setup = deviceManager.getAudioDeviceSetup();
        double sr = setup.sampleRate > 0 ? setup.sampleRate : 44100.0;
        int bs = setup.bufferSize > 0 ? setup.bufferSize : 512;

        auto instance = formatManager.createPluginInstance(desc, sr, bs, error);

        if (!instance)
        {
            player.setProcessor(&hostProcessor);
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Plugin Load Error",
                "Failed to load plugin:\n" + error);
            statusLabel.setText("Error: " + error, juce::dontSendNotification);
            return;
        }

        // Restore plugin state if available
        auto savedSettings = std::unique_ptr<juce::XmlElement>(loadSettings());
        if (savedSettings)
        {
            if (auto* pluginState = savedSettings->getChildByName("PLUGIN_STATE"))
            {
                juce::MemoryBlock mb;
                mb.fromBase64Encoding(pluginState->getAllSubText());
                instance->setStateInformation(mb.getData(), (int)mb.getSize());
            }
        }

        lastPluginId = desc.createIdentifierString();
        hostProcessor.loaded = std::move(instance);

        // AudioProcessorPlayer prepares the host and the newly loaded plugin
        // using the active device's current sample rate and block size.
        player.setProcessor(&hostProcessor);

        auto* proc = hostProcessor.loaded.get();

        if (proc->hasEditor())
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor(proc->createEditor());
            pluginWindow = std::make_unique<PluginWindow>(
                std::move(editor),
                [this] { closePluginWindow(); });
        }

        statusLabel.setText("Loaded: " + desc.name, juce::dontSendNotification);
    }

    void closePluginWindow()
    {
        pluginWindow.reset();
    }

    void saveState()
    {
        juce::XmlElement xml("SETTINGS");

        // Audio device state
        if (auto deviceState = deviceManager.createStateXml())
            xml.addChildElement(new juce::XmlElement(*deviceState.get()));

        // Plugin identifier
        if (lastPluginId.isNotEmpty())
            xml.setAttribute("lastPlugin", lastPluginId);

        // Plugin state
        if (hostProcessor.loaded)
        {
            juce::MemoryBlock mb;
            hostProcessor.loaded->getStateInformation(mb);
            auto* pluginState = xml.createNewChildElement("PLUGIN_STATE");
            pluginState->addTextElement(mb.toBase64Encoding());
        }

        saveSettings(xml);
    }

    void restoreState()
    {
        auto xml = std::unique_ptr<juce::XmlElement>(loadSettings());
        if (!xml) return;

        auto id = xml->getStringAttribute("lastPlugin");
        if (id.isEmpty()) return;

        scanPlugins();

        for (auto& desc : knownPlugins.getTypes())
        {
            if (desc.createIdentifierString() == id)
            {
                loadPlugin(desc);
                return;
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

//==============================================================================
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow()
        : DocumentWindow("Plugin Standalone",
                          juce::Colours::darkgrey,
                          DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

//==============================================================================
class Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Plugin Standalone"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>();
    }

    void shutdown() override { mainWindow.reset(); }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(Application)
