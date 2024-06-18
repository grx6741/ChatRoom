#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <steam_api.h>

#include <raylib.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#define APP_ID 480
#define MAX_CHATMSG_SIZE 1024 * 4

#define LOG( x ) std::cout << ( x ) << std::endl;
#define MIN( x, y ) ((x) < (y) ? (x) : (y))

enum eScreenState {
    LOADING = 0,
    OUTSIDE_LOBBY,
    LOBBY_CREATION,
    LOBBY_JOIN,
    LOBBY
} screen_state;

static struct {
    bool should_quit = false;
    std::string loading_screen_text;
} program;

class Screen {
public:
    static void renderOutsideLobby();
    static void renderLobbyCreation();
    static void renderLobbyJoin();
    static void renderLobby();
    static void renderLoading();
};

class LobbyManager {
public:
    char lobby_name[100]; // text box data, that u type to create a lobby
    char lobby_id_text_box[200]; // same as above, but u type the id to join an existing id
    std::string lobby_leader;
    std::vector<uint64> members;
    std::vector<std::string> messages;

    char chatMsg[MAX_CHATMSG_SIZE];
    uint64 id;

    void CreateLobby();
    void JoinLobby(uint64 SteamID);
    void LeaveLobby();
    void SendMessage(std::string sender, std::string msg);

    void reFillMembersVector();

private:
    // call Values
    // where u handle the lobby creation. Till this is called, we show a Loading Screen
    void OnLobbyCreate( LobbyCreated_t *pCallback, bool bIOFailure );
    CCallResult< LobbyManager, LobbyCreated_t > m_LobbyCreateCallResult;

    void OnLobbyJoin( LobbyEnter_t *pCallback, bool bIOFailure );
    CCallResult< LobbyManager, LobbyEnter_t > m_LobbyJoinCallResult;

    // Call Backs
    STEAM_CALLBACK( LobbyManager, OnLobbyDataUpdate, LobbyChatUpdate_t );
    STEAM_CALLBACK( LobbyManager, OnLobbyMessageRecieved, LobbyChatMsg_t );
};

static LobbyManager lobby_manager;

int main() {
    // This Starts the game in Steam
    if ( SteamAPI_RestartAppIfNecessary( APP_ID ) ) {
        return 1;
    }

    InitWindow(800, 600, "Steam Test");
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    SetExitKey(0);

    if ( !SteamAPI_Init() ) {
        std::cout << "An instance of Steam needs to be running" << std::endl;
        return EXIT_FAILURE;
    }

    screen_state = eScreenState::OUTSIDE_LOBBY;
    SetTargetFPS(100);

    while ( !WindowShouldClose() && !program.should_quit ) {
        BeginDrawing();
        DrawFPS(0, 0);
        SteamAPI_RunCallbacks();
        ClearBackground(RAYWHITE);
        // LOG( screen_state );
        switch ( screen_state ) {
            case eScreenState::LOADING:
                Screen::renderLoading();
                break;
            case eScreenState::OUTSIDE_LOBBY:
                Screen::renderOutsideLobby();
                break;
            case eScreenState::LOBBY_CREATION:
                Screen::renderLobbyCreation();
                break;
            case eScreenState::LOBBY_JOIN:
                Screen::renderLobbyJoin();
                break;
            case eScreenState::LOBBY:
                Screen::renderLobby();
                break;
        }

        EndDrawing();
    }
    lobby_manager.LeaveLobby();
    CloseWindow();
    SteamAPI_Shutdown();
}

void Screen::renderLoading() {
    DrawText(program.loading_screen_text.c_str(), 100, 100, 32, BLACK);
}

void Screen::renderOutsideLobby() {
    if ( GuiButton( Rectangle {0, 0, 300, 50}, "Create Lobby" ) ) {
        screen_state = eScreenState::LOBBY_CREATION;
    }
    if ( GuiButton( Rectangle {0, 100, 300, 50}, "Join Lobby" ) ) {
        screen_state = eScreenState::LOBBY_JOIN;
    }
    if ( GuiButton( Rectangle {0, 200, 300, 50}, "Quit" ) ) {
        program.should_quit = true;
    }
}

void Screen::renderLobbyCreation() {
    float window_width = GetScreenWidth();
    float window_height = GetScreenHeight();

    float w = 0.3 * window_width;
    float h = MIN( 0.1 * window_height, 30 );

    Rectangle textbox_bounds = { 
        static_cast<float>(0.5 * (window_width - w)),
        static_cast<float>(0.5 * (window_height - h)), 
        w, h
    };

    Rectangle button_bounds = {
        static_cast<float>(0.5 * (window_width - w)),
        static_cast<float>(0.5 * (window_height - h)) + 100, 
        w, h
    };

    if ( GuiTextBox( textbox_bounds, lobby_manager.lobby_name, 100, true ) || GuiButton( button_bounds, "Create Lobby" )) {
        if ( strlen( lobby_manager.lobby_name ) > 6 ) {
            screen_state = eScreenState::LOADING;
            program.loading_screen_text = "Creating Lobby";

            lobby_manager.CreateLobby();
        }
    }
}

void Screen::renderLobbyJoin() {
    float window_width = GetScreenWidth();
    float window_height = GetScreenHeight();

    float w = 0.3 * window_width;
    float h = MIN( 0.1 * window_height, 30 );

    Rectangle textbox_bounds = { 
        static_cast<float>(0.5 * (window_width - w)),
        static_cast<float>(0.5 * (window_height - h)), 
        w, h
    };

    Rectangle button_bounds = {
        static_cast<float>(0.5 * (window_width - w)),
        static_cast<float>(0.5 * (window_height - h)) + 100, 
        w, h
    };

    if ( GuiTextBox( textbox_bounds, lobby_manager.lobby_id_text_box, 100, true ) || GuiButton( button_bounds, "Join Lobby" )) {
        if ( strlen( lobby_manager.lobby_id_text_box) > 0 ) {
            screen_state = eScreenState::LOADING;
            program.loading_screen_text = "Joining Lobby";

            lobby_manager.JoinLobby(std::stoull(lobby_manager.lobby_id_text_box));
        }
    }
}

void Screen::renderLobby() {
    Rectangle members_panel = {
        0, 0, 
        static_cast<float>( 0.2 * GetScreenWidth() ), 
        static_cast<float>( GetScreenHeight() )
    };

    Rectangle chat_panel = {
        members_panel.width, 0,
        static_cast<float>( GetScreenWidth() - members_panel.width ),
        members_panel.height
    };

    GuiPanel( members_panel, TextFormat( "Online: %d", lobby_manager.members.size() ) );
    
    float font_height = 20;
    Vector2 padding = {10, 50};
    for (int i = 0; i < lobby_manager.members.size(); i++) {
        float y_offset = i * font_height + 10;
        const char* username = SteamFriends()->GetFriendPersonaName( lobby_manager.members[i] );
        DrawText(username, padding.x, padding.y + y_offset, font_height / 10, BLACK);
    }

    GuiPanel( chat_panel, TextFormat( "%s Lobby: Owned by %s", lobby_manager.lobby_name, lobby_manager.lobby_leader.c_str()) );

    padding = {chat_panel.x + 20, chat_panel.y + 70};
    for (int i = 0; i < lobby_manager.messages.size(); i++) {
        float y_offset = i * font_height + 20;
        DrawText(lobby_manager.messages[i].c_str(), padding.x, padding.y + y_offset, font_height / 2, BLACK);
    }

    float chat_box_height = 50;
    Rectangle chat_box = {
        chat_panel.x,
        static_cast<float>(GetScreenHeight() - chat_box_height),
        chat_panel.width,
        chat_box_height
    };

    if ( GuiTextBox(chat_box, lobby_manager.chatMsg, MAX_CHATMSG_SIZE, true) ) {
        lobby_manager.SendMessage(SteamFriends()->GetPersonaName(), lobby_manager.chatMsg);
        // Clears the text
    }
}

// Lobby Manager Implementation

void LobbyManager::SendMessage( std::string sender, std::string msg ) {
    const char* full_msg = TextFormat( "[%s]: %s", sender.c_str(), msg.c_str() );
    TraceLog( LOG_INFO, "%s : %d", full_msg, msg.size() );

    SteamMatchmaking()->SendLobbyChatMsg( lobby_manager.id, full_msg, strlen(full_msg) );
    std::memset( lobby_manager.chatMsg, 0, strlen(lobby_manager.chatMsg) + 1 );
}

void LobbyManager::CreateLobby() {
    SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby( k_ELobbyTypePublic, 100 );
    m_LobbyCreateCallResult.Set( hSteamAPICall, this, &LobbyManager::OnLobbyCreate );
}

void LobbyManager::OnLobbyCreate( LobbyCreated_t *pCallback, bool bIOFailure ) {

    if ( bIOFailure ) {
        LOG( "[INTERNAL ERROR] Couldn't Create Server" );
    }

    bool failed = true;
    switch ( pCallback->m_eResult ) {
        case k_EResultFail:
            TraceLog( LOG_ERROR, "The server responded, but with an unknown internal error." );
            break;
        case k_EResultTimeout:
            TraceLog( LOG_ERROR, "The message was sent to the Steam servers, but it didn't respond." );
            break;
        case k_EResultLimitExceeded:
            TraceLog( LOG_ERROR, "Your game client has created too many lobbies and is being rate limited." );
            break;
        case k_EResultAccessDenied:
            TraceLog( LOG_ERROR, "Your game isn't set to allow lobbies, or your client does haven't rights to play the game" );
            break;
        case k_EResultNoConnection:
            TraceLog( LOG_ERROR, "Your Steam client doesn't have a connection to the back-end." );
            break;
        case k_EResultOK:
            TraceLog(LOG_INFO, "Created Lobby Succesfully. Lobby ID: %lld", pCallback->m_ulSteamIDLobby);
            failed = false;
            break;
        default:
            TraceLog(LOG_ERROR, "Shouldn't be reachable");
            return;
    }

    if ( failed ) {
        screen_state = eScreenState::OUTSIDE_LOBBY;
        return;
    }

    lobby_manager.id = pCallback->m_ulSteamIDLobby;
    if ( !SteamMatchmaking()->SetLobbyData( (CSteamID) lobby_manager.id, "lobby_name", std::string( lobby_manager.lobby_name ).c_str() ) ) {
        TraceLog(LOG_ERROR, "Invalid Lobby ID");
    }

    // First member in the lobby
    lobby_manager.members.push_back( SteamUser()->GetSteamID().ConvertToUint64() );
    lobby_manager.lobby_leader = SteamFriends()->GetPersonaName();

    if ( !SteamMatchmaking()->SetLobbyData( lobby_manager.id, "lobby_leader", lobby_manager.lobby_leader.c_str() ) ) {
        TraceLog(LOG_ERROR, "Invalid Lobby ID");
    }

    screen_state = eScreenState::LOBBY;

    SetClipboardText( TextFormat( "%lld", lobby_manager.id ) );
}

void LobbyManager::JoinLobby(uint64 SteamID) {
    SteamAPICall_t hSteamAPICall = SteamMatchmaking()->JoinLobby(SteamID);
    m_LobbyJoinCallResult.Set( hSteamAPICall, this, &LobbyManager::OnLobbyJoin );
}

void LobbyManager::reFillMembersVector() {
    lobby_manager.members.clear();
    int nMembers = SteamMatchmaking()->GetNumLobbyMembers( lobby_manager.id );
    for (int i = 0; i < nMembers; i++) {
        lobby_manager.members.push_back( SteamMatchmaking()->GetLobbyMemberByIndex( lobby_manager.id, i ).ConvertToUint64() );
    }
}

void LobbyManager::OnLobbyJoin( LobbyEnter_t *pCallback, bool bIOFailure ) {
    if ( bIOFailure || pCallback->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseError ) {
        TraceLog(LOG_ERROR, "Couldn't Join Lobby");
        screen_state = eScreenState::OUTSIDE_LOBBY;
        return;
    }

    lobby_manager.id = pCallback->m_ulSteamIDLobby;
    lobby_manager.lobby_leader = SteamMatchmaking()->GetLobbyData( lobby_manager.id, "lobby_leader" );
    strncpy( lobby_manager.lobby_name, SteamMatchmaking()->GetLobbyData( lobby_manager.id, "lobby_leader" ), 100 );

    reFillMembersVector();
    messages.clear();

    TraceLog(LOG_INFO, "Joined Lobby %lld", lobby_manager.id);
    SendMessage("SERVER", TextFormat("%s has Joined the lobby", SteamFriends()->GetPersonaName()));
    screen_state = eScreenState::LOBBY;
}

void LobbyManager::LeaveLobby() {
    if (id == 0) {
        TraceLog(LOG_WARNING, "Not Connected to Any Lobby");
        return;
    }
    SteamMatchmaking()->LeaveLobby( this->id );
    this->id = 0;
    this->lobby_leader.clear();
    this->members.clear();
    SendMessage("SERVER", TextFormat("%s has Left the lobby", SteamFriends()->GetPersonaName()));
}

// Call Backs

void LobbyManager::OnLobbyDataUpdate( LobbyChatUpdate_t *pCallback ) {
    uint64 lobby_id = pCallback->m_ulSteamIDLobby;
    uint64 member_id = pCallback->m_ulSteamIDUserChanged;

    reFillMembersVector();

    switch (pCallback->m_rgfChatMemberStateChange) {
        case k_EChatMemberStateChangeEntered:
            break;
        case k_EChatMemberStateChangeLeft:
            break;
    }
}

void LobbyManager::OnLobbyMessageRecieved( LobbyChatMsg_t *pCallback ) {
    if ( pCallback->m_eChatEntryType == 0 ) {
        TraceLog(LOG_ERROR, "Invalid Message recieved");
        return;
    }

    uint32 chatID = pCallback->m_iChatID;
    uint64 lobbyID = pCallback->m_ulSteamIDLobby;
    CSteamID sender = pCallback->m_ulSteamIDUser;

    char recievedMsg[MAX_CHATMSG_SIZE];
    int end = SteamMatchmaking()->GetLobbyChatEntry( lobbyID, chatID, &sender, recievedMsg, MAX_CHATMSG_SIZE, NULL);
    recievedMsg[end] = '\0';

    messages.push_back(recievedMsg);
}
