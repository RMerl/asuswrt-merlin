Public Class DNSServiceBrowser
    Public WithEvents MyEventManager As New Bonjour.DNSSDEventManager

    Private m_service As New Bonjour.DNSSDService
    Private m_browser As Bonjour.DNSSDService
    Private m_resolver As Bonjour.DNSSDService

    Public Sub New()
        MyBase.New()

        'This call is required by the Windows Form Designer.
        InitializeComponent()

        ComboBox1.SelectedIndex = 0
    End Sub
    Public Sub MyEventManager_ServiceFound(ByVal browser As Bonjour.DNSSDService, ByVal flags As Bonjour.DNSSDFlags, ByVal ifIndex As UInteger, ByVal serviceName As String, ByVal regtype As String, ByVal domain As String) Handles MyEventManager.ServiceFound
        Dim browseData As New BrowseData
        browseData.ServiceName = serviceName
        browseData.RegType = regtype
        browseData.Domain = domain
        ServiceNames.Items.Add(browseData)
    End Sub
    Public Sub MyEventManager_ServiceLost(ByVal browser As Bonjour.DNSSDService, ByVal flags As Bonjour.DNSSDFlags, ByVal ifIndex As UInteger, ByVal serviceName As String, ByVal regtype As String, ByVal domain As String) Handles MyEventManager.ServiceLost
        ServiceNames.Items.Remove(serviceName)
    End Sub
    Public Sub MyEventManager_ServiceResolved(ByVal resolver As Bonjour.DNSSDService, ByVal flags As Bonjour.DNSSDFlags, ByVal ifIndex As UInteger, ByVal fullname As String, ByVal hostname As String, ByVal port As UShort, ByVal record As Bonjour.TXTRecord) Handles MyEventManager.ServiceResolved
        m_resolver.Stop()
        m_resolver = Nothing
        Dim browseData As BrowseData = ServiceNames.Items.Item(ServiceNames.SelectedIndex)
        NameField.Text = browseData.ServiceName
        TypeField.Text = browseData.RegType
        DomainField.Text = browseData.Domain
        HostField.Text = hostname
        PortField.Text = port

        If record IsNot Nothing Then
            For i As UInteger = 0 To record.GetCount() - 1
                Dim key As String = record.GetKeyAtIndex(i)
                If key.Length() > 0 Then
                    TextRecord.Items.Add(key + "=" + System.Text.Encoding.ASCII.GetString(record.GetValueAtIndex(i)))
                End If
            Next i
        End If
    End Sub
    Private Sub ClearServiceInfo()
        TextRecord.Items.Clear()
        NameField.Text = ""
        TypeField.Text = ""
        DomainField.Text = ""
        HostField.Text = ""
        PortField.Text = ""
    End Sub
    Private Sub ServiceNames_SelectedIndexChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ServiceNames.SelectedIndexChanged
        If m_resolver IsNot Nothing Then
            m_resolver.Stop()
        End If
        Me.ClearServiceInfo()
        Dim browseData As BrowseData = ServiceNames.Items.Item(ServiceNames.SelectedIndex)
        m_resolver = m_service.Resolve(0, 0, browseData.ServiceName, browseData.RegType, browseData.Domain, MyEventManager)
    End Sub
    Private Sub ComboBox1_SelectedIndexChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ComboBox1.SelectedIndexChanged
        If m_browser IsNot Nothing Then
            m_browser.Stop()
        End If

        ServiceNames.Items.Clear()
        Me.ClearServiceInfo()
        m_browser = m_service.Browse(0, 0, ComboBox1.Items.Item(ComboBox1.SelectedIndex), "", MyEventManager)
    End Sub

    Private Sub ListBox2_SelectedIndexChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextRecord.SelectedIndexChanged

    End Sub
End Class
Public Class BrowseData
    Private m_serviceName As String
    Private m_regType As String
    Private m_domain As String

    Property ServiceName() As String
        Get
            Return m_serviceName
        End Get
        Set(ByVal Value As String)
            m_serviceName = Value
        End Set
    End Property

    Property RegType() As String
        Get
            Return m_regType
        End Get
        Set(ByVal Value As String)
            m_regType = Value
        End Set
    End Property

    Property Domain() As String
        Get
            Return m_domain
        End Get
        Set(ByVal Value As String)
            m_domain = Value
        End Set
    End Property

    Public Overrides Function ToString() As String
        Return m_serviceName
    End Function

End Class
