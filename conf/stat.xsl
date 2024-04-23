<?xml version="1.0" encoding="utf-8" ?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">
    <html>
        <head>
            <title>stream服务状态表</title>
        </head>
        <body>
            <xsl:apply-templates select="vmr"/>
            <hr/>
        </body>
    </html>
</xsl:template>

<xsl:template match="vmr">
    <table cellspacing="1" cellpadding="5">
        <tr bgcolor="#999999">
            <th>stream</th>
            <th>clients</th>
            <th colspan="4">媒体信息</th>
			<th>缓存大小</th>
            <th>缓存时长</th>
            <th>丢包率</th>
            <th>进流量</th>
            <th>出流量</th>
            <th>进带宽</th>
            <th>出带宽</th>
            <th>State</th>
            <th>Time</th>
        </tr>
        <tr>
            <td colspan="2">Accepted: <xsl:value-of select="naccepted"/></td>
            <th bgcolor="#999999">编码方式</th>
            <th bgcolor="#999999">码率</th>
            <th bgcolor="#999999">分辨率</th>
            <th bgcolor="#999999">帧率</th>
		<td>
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="record_len"/>
            </xsl:call-template>
        </td>

        <td align="middle">
            <xsl:value-of select="record_time"/>s
        </td>

        <td align="middle">
            <xsl:value-of select="rtp_loss_rate"/>
        </td>
				
        <td>
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="bytes_in"/>
            </xsl:call-template>
        </td>
        <td>
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="bytes_out"/>
            </xsl:call-template>
        </td>
        <td>
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="bw_in"/>
                <xsl:with-param name="bits" select="1"/>
                <xsl:with-param name="persec" select="1"/>
            </xsl:call-template>
        </td>
        <td>
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="bw_out"/>
                <xsl:with-param name="bits" select="1"/>
                <xsl:with-param name="persec" select="1"/>
            </xsl:call-template>
        </td>
        <td/>
        <td>
            <xsl:call-template name="showtime">
                <xsl:with-param name="time" select="/vmr/uptime"/>
            </xsl:call-template>
        </td>
        </tr>

        <xsl:apply-templates select="server"/>
    </table>
</xsl:template>

<xsl:template match="server">
    <xsl:apply-templates select="app"/>
</xsl:template>

<xsl:template match="app">
    <xsl:apply-templates select="live"/>
    <xsl:apply-templates select="analyzer"/>
</xsl:template>

<xsl:template match="live">
    <tr bgcolor="#aaaaaa">
        <td>live</td>
        <td align="middle">
            <xsl:value-of select="nclients"/>
        </td>
    </tr>
    <xsl:apply-templates select="stream"/>
</xsl:template>

<xsl:template match="analyzer">
    <tr bgcolor="#aaaaaa">
        <td>analyzer</td>
        <td align="middle">
            <xsl:value-of select="nclients"/>
        </td>
    </tr>
    <xsl:apply-templates select="stream"/>
</xsl:template>

<xsl:template match="stream">
    <tr valign="top">
        <xsl:attribute name="bgcolor">
            <xsl:choose>
                <xsl:when test="active">#cccccc</xsl:when>
                <xsl:otherwise>#dddddd</xsl:otherwise>
            </xsl:choose>
        </xsl:attribute>
        <td>
            <a href="">
                <xsl:attribute name="onclick">
                    var d=document.getElementById('<xsl:value-of select="../../name"/>-<xsl:value-of select="name"/>');
                    d.style.display=d.style.display=='none'?'':'none';
                    return false
                </xsl:attribute>
                <xsl:value-of select="name"/>
                <xsl:if test="string-length(name) = 0">
                    [EMPTY]
                </xsl:if>
            </a>
        </td>
        <td align="middle"> <xsl:value-of select="nclients"/> </td>
        <td align="middle">
            <xsl:value-of select="meta/video/codec"/>
        </td>
        <td align="middle">
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="bw_video"/>
            </xsl:call-template>
        </td>
        <td>
            <xsl:apply-templates select="meta/video/width"/>
        </td>
        <td align="middle">
            <xsl:value-of select="meta/video/frame_rate"/>
        </td>


		<td align="middle">
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="record_len"/>
            </xsl:call-template>
        </td>

		<td align="middle">
            <xsl:value-of select="record_time"/>s
        </td>

        <td align="middle">
            <xsl:value-of select="rtp_loss_rate"/>
        </td>

        <td>
            <xsl:call-template name="showsize">
               <xsl:with-param name="size" select="bytes_in"/>
           </xsl:call-template>
        </td>
        <td>
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="bytes_out"/>
            </xsl:call-template>
        </td>
        <td>
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="bw_in"/>
                <xsl:with-param name="bits" select="1"/>
                <xsl:with-param name="persec" select="1"/>
            </xsl:call-template>
        </td>
        <td>
            <xsl:call-template name="showsize">
                <xsl:with-param name="size" select="bw_out"/>
                <xsl:with-param name="bits" select="1"/>
                <xsl:with-param name="persec" select="1"/>
            </xsl:call-template>
        </td>
        <td><xsl:call-template name="streamstate"/></td>
        <td>
            <xsl:call-template name="showtime">
               <xsl:with-param name="time" select="time"/>
            </xsl:call-template>
        </td>

    </tr>
    <tr style="display:none">
        <xsl:attribute name="id">
            <xsl:value-of select="../../name"/>-<xsl:value-of select="name"/>
        </xsl:attribute>
        <td colspan="16" ngcolor="#eeeeee">
            <table cellspacing="1" cellpadding="5">
                <tr>
                    <th>Id</th>
                    <th>State</th>
                    <th>Address</th>
                    <th>Schema</th>
                    <th>Page URL</th>
                    <th>Time</th>
                </tr>
                <xsl:apply-templates select="client"/>
            </table>
        </td>
    </tr>
</xsl:template>

<xsl:template name="showtime">
    <xsl:param name="time"/>
    <xsl:if test="$time &gt; 0">
        <xsl:variable name="sec">
            <xsl:value-of select="floor($time div 1000)"/>
        </xsl:variable>
        <xsl:if test="$sec &gt;= 86400">
            <xsl:value-of select="floor($sec div 86400)"/>d
        </xsl:if>
        <xsl:if test="$sec &gt;= 3600">
            <xsl:value-of select="(floor($sec div 3600)) mod 24"/>h
        </xsl:if>
        <xsl:if test="$sec &gt;= 60">
            <xsl:value-of select="(floor($sec div 60)) mod 60"/>m
        </xsl:if>
        <xsl:value-of select="$sec mod 60"/>s
    </xsl:if>
</xsl:template>

<xsl:template name="showsize">
    <xsl:param name="size"/>
    <xsl:param name="bits" select="0" />
    <xsl:param name="persec" select="0" />
    <xsl:variable name="sizen">
        <xsl:value-of select="floor($size div 1024)"/>
    </xsl:variable>
    <xsl:choose>
        <xsl:when test="$sizen &gt;= 1073741824">
            <xsl:value-of select="format-number($sizen div 1073741824,'#.###')"/>T</xsl:when>
        <xsl:when test="$sizen &gt;= 1048576">
            <xsl:value-of select="format-number($sizen div 1048576,'#.###')"/>G</xsl:when>
        <xsl:when test="$sizen &gt;= 1024">
            <xsl:value-of select="format-number($sizen div 1024,'#.##')"/>M</xsl:when>
        <xsl:when test="$sizen &gt;= 0">
            <xsl:value-of select="$sizen"/>K</xsl:when>
    </xsl:choose>
    <xsl:if test="string-length($size) &gt; 0">
        <xsl:choose>
            <xsl:when test="$bits = 1">b</xsl:when>
            <xsl:otherwise>B</xsl:otherwise>
        </xsl:choose>
        <xsl:if test="$persec = 1">ps</xsl:if>
    </xsl:if>
</xsl:template>

<xsl:template name="streamstate">
    <xsl:choose>
        <xsl:when test="active">active</xsl:when>
        <xsl:otherwise>idle</xsl:otherwise>
    </xsl:choose>
</xsl:template>


<xsl:template name="clientstate">
    <xsl:choose>
        <xsl:when test="publishing">publishing</xsl:when>
        <xsl:otherwise>playing</xsl:otherwise>
    </xsl:choose>
</xsl:template>


<xsl:template match="client">
    <tr>
        <xsl:attribute name="bgcolor">
            <xsl:choose>
                <xsl:when test="publishing">#cccccc</xsl:when>
                <xsl:otherwise>#eeeeee</xsl:otherwise>
            </xsl:choose>
        </xsl:attribute>
        <td><xsl:value-of select="id"/></td>
        <td><xsl:call-template name="clientstate"/></td>
		<td><xsl:value-of select="address"/></td>
        <td><xsl:value-of select="schema"/></td>
		<td><xsl:value-of select="pageurl"/></td>
        <td>
            <xsl:call-template name="showtime">
               <xsl:with-param name="time" select="time"/>
            </xsl:call-template>
        </td>
    </tr>
</xsl:template>

<xsl:template match="publishing">
    publishing
</xsl:template>

<xsl:template match="active">
    active
</xsl:template>

<xsl:template match="width">
    <xsl:value-of select="."/>x<xsl:value-of select="../height"/>
</xsl:template>

</xsl:stylesheet>
